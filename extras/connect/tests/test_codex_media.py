import asyncio
import contextlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock

from aiohttp import web
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from codex_bridge import CodexBridge, card, messages, public_item, completion
from media_controls import MediaControls

ONE='00000000-0000-7000-8000-000000000001'
TWO='00000000-0000-7000-8000-000000000002'


class CodexTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='jlinux-codex-test-')
        self.socket=Path(self.temp.name)/'codex.sock'
        self.calls=[];self.replies=[];self.requests=False;self.direct=True
        self.active=True
        self.turn_status="inProgress"
        self.turn_ident="turn-one"
        self.thread={'id':ONE,'name':'Agente de teste','cwd':self.temp.name,'status':{'type':'active','activeFlags':[]},'canAcceptDirectInput':True}
        async def connection(request):
            ws=web.WebSocketResponse();await ws.prepare(request)
            self.connection = ws
            async for message in ws:
                data=message.json()
                if 'method' not in data:
                    self.replies.append(data);continue
                method,params=data['method'],data.get('params',{})
                if 'id' not in data:continue
                self.calls.append((method,params))
                thread=dict(self.thread,canAcceptDirectInput=self.direct,status={'type':'active' if self.active else 'idle','activeFlags':[]})
                if method=='initialize':result={'userAgent':'test'}
                elif method=='thread/list':result={'data':[thread],'nextCursor':None}
                elif method in ('thread/read','thread/resume','thread/start'):result={'thread':thread}
                elif method=='thread/turns/list':result={'data':[{'id':self.turn_ident,'status':self.turn_status,'startedAt':1,'items':[
                    {'id':'user','type':'userMessage','content':[{'type':'text','text':'Meu prompt'}]},
                    {'id':'progress','type':'agentMessage','phase':'commentary','text':'Vou abrir arquivos e testar'},
                    {'id':'command','type':'commandExecution','aggregatedOutput':'PRIVATE_TOOL_OUTPUT'},
                    {'id':'reason','type':'reasoning','text':'PRIVATE_REASONING'},
                    {'id':'answer','type':'agentMessage','phase':'final_answer','text':'Resposta final do agente.'}]}], 'nextCursor':None}
                elif method=='thread/items/list':result={'data':[
                    {'turnId':'turn-one','item':{'id':'answer','type':'agentMessage','phase':'final_answer','text':'Resposta final do agente.'}},
                    {'turnId':'turn-one','item':{'id':'tool','type':'commandExecution','aggregatedOutput':'PRIVATE_TOOL_OUTPUT'}},
                    {'turnId':'turn-one','item':{'id':'progress','type':'agentMessage','phase':'commentary','text':'Vou abrir arquivos'}},
                    {'turnId':'turn-one','item':{'id':'user','type':'userMessage','content':[{'type':'text','text':'Meu prompt'}]}}], 'nextCursor':None}
                elif method in ('turn/start','turn/steer'):result={'turnId':'turn-one'}
                else:result={}
                await ws.send_json({'id':data['id'],'result':result})
                if method=='thread/resume' and self.requests:
                    await ws.send_json({'id':101,'method':'item/tool/requestUserInput','params':{
                        'threadId':ONE,'itemId':'question','turnId':'turn-one','questions':[{'id':'color','question':'Qual cor?', 'options':[{'label':'Rosa','description':'rosa'}]}]}})
                    await ws.send_json({'id':102,'method':'item/commandExecution/requestApproval','params':{
                        'threadId':ONE,'itemId':'command','turnId':'turn-one','command':'echo autorizado','reason':'Confirmação humana'}})
            return ws
        app=web.Application();app.router.add_get('/',connection)
        self.runner=web.AppRunner(app);await self.runner.setup();await web.UnixSite(self.runner,str(self.socket)).start()
        self.bridge=CodexBridge(self.socket)

    async def asyncTearDown(self):
        await self.bridge.close();await self.runner.cleanup();self.temp.cleanup()

    async def test_actual_transport_filters_tools_and_uses_runtime_state(self):
        listing=await self.bridge.listing()
        self.assertEqual(listing['agents'][0]['state'],'working')
        detail=await self.bridge.detail(ONE)
        self.assertEqual([item['role'] for item in detail['messages']],['user','agent'])
        text=json.dumps(detail)
        for hidden in ('PRIVATE_TOOL_OUTPUT','PRIVATE_REASONING','Vou abrir arquivos'):
            self.assertNotIn(hidden,text)
        await self.bridge.prompt(ONE,'continue com o projeto')
        self.assertEqual(self.calls[-1][0],'turn/steer')
        self.assertEqual(self.calls[-1][1]['expectedTurnId'],'turn-one')
        self.assertNotIn('approvalPolicy',self.calls[-1][1])
        self.active=False
        await self.bridge.prompt(ONE,'nova tarefa')
        self.assertEqual(self.calls[-1][0],'turn/start')
        self.direct=False
        with self.assertRaises(web.HTTPConflict):await self.bridge.prompt(ONE,'não permitido')
        self.assertEqual(card({'id':ONE,'status':{'type':'active','activeFlags':['waitingOnUserInput']}})['state'],'waiting')

    async def test_completed_status_uses_successful_turn_and_caches_metadata(self):
        self.assertIsNone((await self.bridge.listing())['agents'][0]['completion'])
        self.active = False
        self.turn_status = 'completed'
        self.assertEqual((await self.bridge.listing())['agents'][0]['completion'], 'turn-one')
        count = sum(method == 'thread/turns/list' for method, _ in self.calls)
        self.assertEqual((await self.bridge.listing())['agents'][0]['completion'], 'turn-one')
        self.assertEqual(count, sum(method == 'thread/turns/list' for method, _ in self.calls))
        self.thread['updatedAt'] = 2
        self.turn_ident = 'turn-two'
        self.assertEqual((await self.bridge.listing())['agents'][0]['completion'], 'turn-two')
        self.assertEqual((await self.bridge.detail(ONE))['agent']['completion'], 'turn-two')
        for state in ('interrupted', 'failed', 'inProgress'):
            self.assertIsNone(completion({'status': {'type': 'idle'}}, {'data': [{'id':'turn', 'status':state}]}))
        self.assertIsNone(completion({'status': {'type': 'idle'}}, {'data': []}))
        self.assertIsNone(completion({'status': {'type': 'active'}}, {'data': [{'id':'turn', 'status':'completed'}]}))

    async def test_socket_delivers_text_notifications(self):
        await self.bridge.detail(ONE)
        changed = asyncio.Event()
        self.bridge.listeners[ONE] = {changed}
        await self.connection.send_json({'method': 'item/started', 'params': {'threadId': ONE,
            'item': {'id': 'stream', 'type': 'agentMessage', 'phase': 'final_answer', 'text': ''}}})
        await asyncio.wait_for(changed.wait(), 2)
        changed.clear()
        await self.connection.send_json({'method': 'item/agentMessage/delta', 'params': {
            'threadId': ONE, 'itemId': 'stream', 'turnId': 'turn-one', 'delta': 'Ao vivo pelo socket'}})
        await asyncio.wait_for(changed.wait(), 2)
        self.assertEqual(self.bridge.live[ONE]['stream']['text'], 'Ao vivo pelo socket')
        optout = next(params for method, params in self.calls if method == 'initialize')['capabilities']['optOutNotificationMethods']
        self.assertNotIn('item/agentMessage/delta', optout)

    async def test_live_text_deltas_filter_tools_commentary_and_other_threads(self):
        await self.bridge.detail(ONE)
        changed = asyncio.Event()
        self.bridge.listeners[ONE] = {changed}
        emit = self.bridge.message_event
        for method, params in [
            ('item/started', {'threadId': ONE, 'item': {'id': 'comment', 'type': 'agentMessage', 'phase': 'commentary', 'text': ''}}),
            ('item/agentMessage/delta', {'threadId': ONE, 'itemId': 'comment', 'delta': 'PRIVATE_PROGRESS'}),
            ('item/completed', {'threadId': ONE, 'item': {'id': 'tool', 'type': 'commandExecution', 'aggregatedOutput': 'PRIVATE_TOOL'}}),
            ('item/started', {'threadId': TWO, 'item': {'id': 'other', 'type': 'agentMessage', 'text': 'OTHER_THREAD'}}),
            ('item/agentMessage/delta', {'threadId': ONE, 'itemId': 'unknown', 'delta': 'UNKNOWN_PHASE'})]:
            emit(method, params)
        self.assertFalse(changed.is_set())
        emit('item/started', {'threadId': ONE, 'item': {'id': 'live', 'type': 'agentMessage', 'phase': 'final_answer', 'text': ''}})
        changed.clear()
        emit('item/agentMessage/delta', {'threadId': ONE, 'itemId': 'live', 'delta': 'Resposta '})
        emit('item/agentMessage/delta', {'threadId': ONE, 'itemId': 'live', 'delta': 'ao vivo'})
        self.assertTrue(changed.is_set())
        detail = await self.bridge.detail(ONE)
        self.assertEqual(detail['messages'][-1]['text'], 'Resposta ao vivo')
        self.assertNotIn('PRIVATE_', json.dumps(detail))
        emit('item/completed', {'threadId': ONE, 'item': {'id': 'live', 'type': 'agentMessage', 'phase': 'final_answer', 'text': 'Resposta ao vivo completa.'}})
        self.assertEqual((await self.bridge.detail(ONE))['messages'][-1]['text'], 'Resposta ao vivo completa.')

    async def test_questions_and_explicit_single_use_approvals(self):
        self.requests=True
        detail=await self.bridge.detail(ONE)
        self.assertEqual(detail['agent']['state'],'waiting')
        question=next(item for item in detail['requests'] if item['kind']=='question')
        approval=next(item for item in detail['requests'] if item['kind']=='approval')
        self.assertEqual(self.replies,[])
        with self.assertRaises(web.HTTPConflict):await self.bridge.reply(TWO,question['id'],{'answers':{'color':'Rosa'}})
        with self.assertRaises(web.HTTPBadRequest):await self.bridge.reply(ONE,question['id'],{'answers':{}})
        await self.bridge.reply(ONE,question['id'],{'answers':{'color':'Rosa'}})
        await self.bridge.reply(ONE,approval['id'],{'decision':'accept'})
        await asyncio.sleep(.01)
        self.assertEqual(self.replies[0],{'id':101,'result':{'answers':{'color':{'answers':['Rosa']}}}})
        self.assertEqual(self.replies[1],{'id':102,'result':{'decision':'accept'}})
        with self.assertRaises(web.HTTPConflict):await self.bridge.reply(ONE,approval['id'],{'decision':'accept'})

    async def test_async_questions_match_the_installed_tui_protocol(self):
        question={'turnId':'turn-one','item':{'id':'call_question','type':'agentMessage','delivery':'async',
                   'questions':[{'title':'Qual cor?','options':['Rosa','Azul']}]}}
        self.bridge.sync_questions(ONE,[question],'turn-one')
        pending=self.bridge.waiting(ONE)[0]
        await self.bridge.reply(ONE,pending['id'],{'answers':{'0':'Rosa'}})
        self.assertEqual(self.calls[-1][0],'turn/steer')
        text=self.calls[-1][1]['input'][0]['text']
        self.assertIn('send_user_message_question_reply',text)
        reply={'turnId':'turn-one','item':{'id':'user-reply','type':'userMessage','content':[{'type':'text','text':text}]}}
        self.bridge.sync_questions(ONE,[question,reply],'turn-one')
        self.assertEqual(self.bridge.waiting(ONE),[])
        self.assertEqual(public_item(reply)['text'],'Qual cor?: Rosa')
        self.bridge.async_answers.clear()
        self.bridge.sync_questions(ONE,[reply,question],'turn-one')
        self.assertEqual(self.bridge.waiting(ONE),[])

    async def test_stale_socket_recovers_without_restarting_existing_work(self):
        original=self.bridge.open_socket
        attempts=0
        async def open_socket():
            nonlocal attempts
            attempts+=1
            if attempts==1:raise OSError('stale socket')
            await original()
        self.bridge.open_socket=open_socket
        self.bridge.allow_start=True
        self.bridge.start_daemon=AsyncMock()
        data=await self.bridge.listing()
        self.assertEqual(len(data['agents']),1)
        self.bridge.start_daemon.assert_awaited_once()
        self.assertFalse(any(method.startswith('turn/') for method,_ in self.calls))

    async def test_input_validation_and_create_without_starting_a_turn(self):
        for ident,text in [('../etc/passwd','prompt'),(ONE,''),(ONE,'x'*32001)]:
            with self.assertRaises(web.HTTPBadRequest):await self.bridge.prompt(ident,text)
        self.assertEqual(self.calls,[])
        created=await self.bridge.create(self.temp.name)
        self.assertEqual(created['id'],ONE)
        self.assertEqual(self.calls[-1][0],'thread/start')
        self.assertFalse(any(method.startswith('turn/') for method,_ in self.calls))
        with self.assertRaises(web.HTTPBadRequest):await self.bridge.create('relative')


class MediaTests(unittest.IsolatedAsyncioTestCase):
    async def test_mpris_controls_capabilities_and_fixed_methods(self):
        controls=MediaControls(lambda:{'DBUS_SESSION_BUS_ADDRESS':f'unix:path=/run/user/{os.getuid()}/bus'})
        calls=[]
        async def call(address,name,interface,method,*args):
            calls.append((name,interface,method,args))
            if method=='ListNames':return {'data':[['org.mpris.MediaPlayer2.test','unsafe;command']]}
            if method=='GetAll':return {'data':[{
                'PlaybackStatus':{'data':'Playing'},'CanControl':{'data':True},'CanPause':{'data':True},
                'CanPlay':{'data':True},'CanSeek':{'data':True},'CanGoNext':{'data':True},'CanGoPrevious':{'data':False},
                'Metadata':{'data':{'xesam:title':{'data':'Minha música'},'xesam:artist':{'data':['Artista']}}}}]}
            return {}
        controls.call=call
        data=await controls.status();player=data['players'][0]
        self.assertEqual(player['title'],'Minha música');self.assertTrue(player['playing']);self.assertFalse(player['can_previous'])
        await controls.action(player['id'],'play_pause');self.assertEqual(calls[-1][2],'PlayPause')
        await controls.action(player['id'],'next');self.assertEqual(calls[-1][2],'Next')
        await controls.action(player['id'],'seek_back');self.assertEqual(calls[-1][2:4],('Seek',('x','-10000000')))
        await controls.action(player['id'],'seek_forward');self.assertEqual(calls[-1][2:4],('Seek',('x','10000000')))
        with self.assertRaises(web.HTTPConflict):await controls.action(player['id'],'previous')
        for action in ['Next;reboot',[],None]:
            with self.assertRaises(web.HTTPBadRequest):await controls.action(player['id'],action)
