#!/usr/bin/env python3
"""Capture/input integration on a private compositor and an isolated mobile browser."""
import asyncio
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile

from aiohttp import web

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location('connect_server', ROOT / 'server.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


async def main():
    with tempfile.TemporaryDirectory(prefix='jlinux-connect-desktop-') as directory:
        root = Path(directory)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=str(root / 'config'),
                   WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
                   JLINUX_REMOTE_CONTROL='1')
        processes = []
        runner = None
        with (root / 'session.log').open('w') as log:
            try:
                compositor = await asyncio.create_subprocess_exec(str(Path(sys.argv[1]).resolve()), env=env, stdout=log, stderr=log)
                processes.append(compositor)
                async with asyncio.timeout(8):
                    while 'JLinux compositor pronto:' not in (root / 'session.log').read_text():
                        assert compositor.returncode is None
                        await asyncio.sleep(.05)
                env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
                foot = await asyncio.create_subprocess_exec('foot', '--config=/dev/null', '--title=Connect isolated fixture',
                       'bash', '--noprofile', '--norc', env=env, stdout=log, stderr=log)
                processes.append(foot)
                async with asyncio.timeout(5):
                    while 'Janela mapeada: Connect isolated fixture' not in (root / 'session.log').read_text():
                        await asyncio.sleep(.05)
                home = root / 'files'
                home.mkdir()
                (home / 'do-pc.txt').write_text('Arquivo do computador')
                companion = module.Companion(root / 'state', ROOT / 'web', desktop=lambda: env, home=home)
                level = .42
                async def command(*args):
                    nonlocal level
                    if 'set-volume' in args:
                        level = float(args[-1])
                        (root / 'volume-marker').write_text(str(level))
                    return f'Volume: {level}\n'
                companion.command = command
                updating = False
                async def system_status(logs=False):
                    return {'running': updating, 'success': None, 'state': 'running' if updating else 'idle', 'log': 'Atualização simulada' if updating else ''}
                async def system_action(action):
                    nonlocal updating
                    with (root / 'actions').open('a') as marker:
                        marker.write(action + '\n')
                    updating = action == 'update'
                    return {'ok': True, 'action': action}
                companion.system.status = system_status
                companion.system.execute = system_action
                media_playing = True
                async def media_status():
                    return {'players':[{'id':'mock-player','name':'Player de teste','title':'Música de teste','artist':'Artista',
                        'playing':media_playing,'can_play_pause':True,'can_seek':True,'can_previous':True,'can_next':True}]}
                async def media_action(player, action):
                    nonlocal media_playing
                    if action=='play_pause':media_playing=not media_playing
                    with (root/'media-actions').open('a') as output:output.write(action+'\n')
                    return {'ok':True}
                companion.media.status=media_status;companion.media.action=media_action
                ids=['00000000-0000-7000-8000-000000000001','00000000-0000-7000-8000-000000000002']
                answers=[];prompts=[]
                def agent(ident):
                    return {'id':ident,'title':'Agente aguardando' if ident==ids[0] else 'Agente trabalhando','cwd':str(root),
                        'state':'waiting' if ident==ids[0] and not answers else 'working' if ident==ids[1] else 'idle','can_send':True,'updated':1,'parent':None}
                async def codex_list(cursor=None, search=''):
                    return {'agents':[agent(ident) for ident in ids] + [
                        {**agent(ids[1]),'id':'00000000-0000-7000-8000-000000000003','title':'Outro projeto','cwd':'/outro/'+root.name},
                        {**agent(ids[1]),'id':'00000000-0000-7000-8000-000000000004','title':'Sem projeto','cwd':''}], 'cursor':None}
                async def codex_detail(ident,cursor=None):
                    companion.codex.subscribed.add(ident)
                    chat=[{'id':'user','role':'user','text':'Trabalhe no projeto','time':1},
                          {'id':'agent','role':'agent','text':'Resposta final organizada.','time':2}]
                    if prompts:chat.extend([{'id':'last-user','role':'user','text':prompts[-1],'time':3},
                                            {'id':'last-agent','role':'agent','text':'Prompt recebido pelo agente.','time':4}])
                    requests=[] if answers or ident!=ids[0] else [{'id':'question-one','kind':'question','questions':[
                        {'id':'style','title':'Qual estilo você prefere?','secret':False,'options':['Rosa','Azul']}]}]
                    return {'agent':agent(ident),'messages':chat,'requests':requests,'cursor':None}
                async def codex_prompt(ident,text):
                    prompts.append(text);(root/'codex-prompt').write_text(text)
                    async def stream_answer():
                        await asyncio.sleep(.2)
                        emit=companion.codex.message_event
                        emit('item/started',{'threadId':ident,'item':{'id':'live-answer','type':'agentMessage','phase':'final_answer','text':''}})
                        emit('item/agentMessage/delta',{'threadId':ident,'itemId':'live-answer','delta':'Texto parcial ao vivo'})
                        emit('item/completed',{'threadId':ident,'item':{'id':'private-tool','type':'commandExecution','aggregatedOutput':'PRIVATE_STREAM_OUTPUT'}})
                        await asyncio.sleep(1)
                        emit('item/completed',{'threadId':ident,'item':{'id':'live-answer','type':'agentMessage','phase':'final_answer','text':'Texto parcial ao vivo: resposta completa.'}})
                    asyncio.create_task(stream_answer())
                    return {'ok':True}
                async def codex_reply(ident,token,data):
                    answers.append(data);return {'ok':True}
                companion.codex.listing=codex_list;companion.codex.detail=codex_detail
                companion.codex.prompt=codex_prompt;companion.codex.reply=codex_reply
                runner = web.AppRunner(companion.application(), access_log=None)
                await runner.setup()
                site = web.TCPSite(runner, '127.0.0.1', 0)
                await site.start()
                port = site._server.sockets[0].getsockname()[1]
                companion.hosts.add(f'127.0.0.1:{port}')
                fixture = root / 'fixture.json'
                fixture.write_text(json.dumps({'url': f'http://127.0.0.1:{port}', 'key': companion.key,
                                               'marker': str(root / 'remote-keyboard-marker'), 'volumeMarker': str(root / 'volume-marker'),
                                               'fileRoot': str(home), 'actions': str(root / 'actions'), 'mediaActions':str(root/'media-actions'), 'codexPrompt':str(root/'codex-prompt')}))
                browser = await asyncio.create_subprocess_exec('node', str(ROOT / 'tests/smoke-mobile.mjs'), str(fixture))
                processes.append(browser)
                assert await asyncio.wait_for(browser.wait(), 55) == 0, 'Mobile browser integration failed'
            except Exception:
                print((root / 'session.log').read_text(), file=sys.stderr)
                raise
            finally:
                if runner:
                    await runner.cleanup()
                for process in reversed(processes):
                    if process.returncode is None:
                        with contextlib.suppress(ProcessLookupError):
                            process.terminate()
                        await process.wait()


asyncio.run(main())
