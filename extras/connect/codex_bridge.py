"""Typed, filtered access to the existing local Codex daemon; never exposes raw RPC."""
import asyncio
from collections import OrderedDict
import contextlib
import json
import os
from pathlib import Path
import re
import secrets

from aiohttp import ClientSession, ClientTimeout, ClientError, UnixConnector, WSMsgType, web

KINDS = ['cli', 'vscode', 'exec', 'appServer', 'subAgent', 'subAgentReview',
         'subAgentCompact', 'subAgentThreadSpawn', 'subAgentOther', 'unknown']
APPROVALS = {'item/commandExecution/requestApproval': 'command',
             'item/fileChange/requestApproval': 'file', 'item/permissions/requestApproval': 'permissions'}


def thread_id(value):
    if not isinstance(value, str) or not re.fullmatch(r'[a-fA-F0-9]{8}(?:-[a-fA-F0-9]{4}){3}-[a-fA-F0-9]{12}', value):
        raise web.HTTPBadRequest(text='Agente inválido')
    return value


def card(thread):
    status = thread.get('status', {})
    flags = status.get('activeFlags', [])
    state = ('waiting' if any(flag in flags for flag in ('waitingOnApproval', 'waitingOnUserInput'))
             else 'working' if status.get('type') == 'active'
             else 'error' if status.get('type') == 'systemError' else 'idle')
    return {'id': thread['id'], 'title': (thread.get('name') or thread.get('agentNickname') or
            thread.get('preview') or 'Novo agente').splitlines()[0][:140],
            'state': state, 'status': status.get('type'), 'cwd': thread.get('cwd', ''),
            'updated': thread.get('updatedAt', 0), 'parent': thread.get('parentThreadId'),
            'role': thread.get('agentRole'), 'can_send': thread.get('canAcceptDirectInput', True)}


def messages(turns):
    output = []
    for turn in reversed(turns):
        for item in turn.get('items', []):
            kind = item.get('type')
            if kind == 'userMessage':
                text = '\n'.join(part.get('text', '') for part in item.get('content', []) if part.get('type') == 'text')
                if text.startswith(('<environment_context>', '<permissions instructions>')):
                    continue
                role = 'user'
            elif kind == 'agentMessage' and item.get('phase') != 'commentary':
                text, role = item.get('text', ''), 'agent'
            else:
                continue
            if text.strip():
                output.append({'id': item.get('id'), 'role': role, 'text': text,
                               'time': turn.get('completedAt') or turn.get('startedAt')})
    return output


def question_replies(text):
    match = re.search(r'<send_user_message_question_reply>\s*(.*?)\s*</send_user_message_question_reply>', text, re.S)
    if not match:
        return []
    try:
        replies = json.loads(match[1])
        return replies if isinstance(replies, list) else []
    except ValueError:
        return []


def public_item(row):
    item = row['item']
    result = messages([{'startedAt': (row.get('startedAtMs') or 0)/1000, 'items': [item]}])
    if not result:
        return None
    message = result[0]
    replies = question_replies(message['text']) if message['role'] == 'user' else []
    if replies:
        message['text'] = '\n'.join(str(reply.get('question', 'Resposta')) + ': ' + str(reply.get('answer', '')) for reply in replies)
    return message


class CodexBridge:
    def __init__(self, socket=None):
        self.allow_start = socket is None
        self.socket = Path(socket) if socket else Path.home() / '.codex/app-server-control/app-server-control.sock'
        self.client = self.ws = self.reader = None
        self.lock = asyncio.Lock()
        self.pending = {}
        self.requests = {}
        self.subscribed = set()
        self.sequence = 0
        self.thread_locks = {}
        self.histories = OrderedDict()
        self.async_answers = set()
        self.question_heads = {}

    async def open_socket(self):
        self.client = ClientSession(connector=UnixConnector(path=str(self.socket)), timeout=ClientTimeout(total=5))
        try:
            self.ws = await self.client.ws_connect('http://localhost/', heartbeat=30, max_msg_size=32 * 1024**2)
        except BaseException:
            await self.client.close()
            self.client = None
            raise

    async def start_daemon(self):
        command = Path.home() / '.local/bin/codex'
        if not command.exists():
            raise web.HTTPServiceUnavailable(text='O Codex não está instalado neste computador')
        runtime = f'/run/user/{os.getuid()}'
        env = dict(os.environ, XDG_RUNTIME_DIR=runtime, DBUS_SESSION_BUS_ADDRESS=f'unix:path={runtime}/bus')
        process = await asyncio.create_subprocess_exec('/usr/bin/systemctl', '--user', 'start', 'jlinux-codex.service',
                  env=env, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        try:
            if await asyncio.wait_for(process.wait(), 15):
                raise web.HTTPServiceUnavailable(text='Não foi possível iniciar o serviço local do Codex')
        except TimeoutError:
            with contextlib.suppress(ProcessLookupError):
                process.terminate()
            await process.wait()
            raise web.HTTPServiceUnavailable(text='O Codex está demorando para iniciar')

    async def connect(self):
        async with self.lock:
            if self.ws and not self.ws.closed:
                return
            await self.close()
            try:
                try:
                    await self.open_socket()
                except (OSError, ClientError):
                    if not self.allow_start:
                        raise
                    await self.start_daemon()
                    async with asyncio.timeout(10):
                        while True:
                            try:
                                await self.open_socket()
                                break
                            except (OSError, ClientError):
                                await asyncio.sleep(.1)
                self.reader = asyncio.create_task(self.read())
                await self.call('initialize', {'clientInfo': {'name': 'jlinux_connect', 'title': 'JLinux Connect', 'version': '1'},
                    'capabilities': {'experimentalApi': True, 'requiredHumanApproval': True,
                        'optOutNotificationMethods': ['item/commandExecution/outputDelta', 'item/fileChange/outputDelta',
                            'item/reasoning/textDelta', 'item/reasoning/summaryTextDelta',
                            'item/started', 'item/completed', 'item/agentMessage/delta']}}, connected=True)
                await self.ws.send_json({'method': 'initialized'})
            except Exception:
                await self.close()
                raise web.HTTPServiceUnavailable(text='Não foi possível conectar ao Codex local. Verifique o Codex no computador.')

    async def read(self):
        socket = self.ws
        try:
            async for message in socket:
                if message.type != WSMsgType.TEXT:
                    continue
                data = message.json()
                if 'id' in data and 'method' not in data:
                    future = self.pending.get(data['id'])
                    if future and not future.done():
                        future.set_result(data)
                elif 'id' in data and 'method' in data:
                    params = data.get('params', {})
                    if params.get('threadId') and not any(item['id'] == data['id'] for item in self.requests.values()):
                        token = secrets.token_urlsafe(18)
                        self.requests[token] = {'id': data['id'], 'method': data['method'], 'params': params}
                elif data.get('method') == 'serverRequest/resolved':
                    resolved = data.get('params', {})
                    self.requests = {token: request for token, request in self.requests.items()
                                     if request['id'] != resolved.get('requestId')}
        except (OSError, ValueError):
            pass
        finally:
            await socket.close()
            for future in self.pending.values():
                if not future.done():
                    future.set_exception(ConnectionError('Codex desconectado'))

    async def call(self, method, params, connected=False):
        if not connected:
            await self.connect()
        self.sequence += 1
        ident = self.sequence
        future = asyncio.get_running_loop().create_future()
        self.pending[ident] = future
        try:
            await self.ws.send_json({'id': ident, 'method': method, 'params': params})
            response = await asyncio.wait_for(future, 20)
            if 'error' in response:
                raise web.HTTPConflict(text=str(response['error'].get('message', 'O Codex recusou a solicitação'))[:1000])
            return response.get('result', {})
        except (OSError, TimeoutError, ConnectionError, RuntimeError):
            raise web.HTTPServiceUnavailable(text='A conexão com o Codex foi interrompida. Tente novamente.')
        finally:
            self.pending.pop(ident, None)

    async def listing(self, cursor=None, search=''):
        data = await self.call('thread/list', {'limit': 50, 'cursor': cursor, 'searchTerm': search or None,
                  'sortKey': 'updated_at', 'sortDirection': 'desc', 'sourceKinds': KINDS, 'currentAccountOnly': True})
        cards = []
        for thread in data.get('data', []):
            info = card(thread)
            if info['status'] == 'active':
                turns = await self.call('thread/turns/list', {'threadId': info['id'], 'limit': 1, 'itemsView': 'summary'})
                for turn in turns.get('data', []):
                    for item in turn.get('items', []):
                        if item.get('questions') and item.get('delivery') == 'async':
                            if all((info['id'], item['id'], index) in self.async_answers for index in range(len(item['questions']))):
                                continue
                            anchor = self.question_heads.get((info['id'], item['id']), item['id'])
                            rows = []
                            cursor = {'type': 'item', 'itemId': anchor}
                            for _ in range(40):
                                page = await self.call('thread/items/list', {'threadId': info['id'], 'turnId': turn['id'],
                                    'cursor': cursor, 'sortDirection': 'asc', 'limit': 100, 'deferOutput': {}})
                                rows.extend(page.get('data', []))
                                cursor = page.get('nextCursor')
                                if not cursor:
                                    break
                            if rows:
                                self.question_heads[(info['id'], item['id'])] = rows[-1]['item']['id']
                            self.sync_questions(info['id'], [{'turnId': turn['id'], 'item': item}, *rows], turn['id'])
            if info['status'] != 'active':
                self.sync_questions(info['id'], [], None)
            if self.waiting(info['id']):
                info['state'] = 'waiting'
            cards.append(info)
        return {'agents': cards, 'cursor': data.get('nextCursor')}

    def sync_questions(self, ident, rows, active):
        for row in rows:
            item = row['item']
            if item.get('type') != 'userMessage':
                continue
            text = '\n'.join(part.get('text', '') for part in item.get('content', []) if part.get('type') == 'text')
            for reply in question_replies(text):
                try:
                    method, call, index = json.loads(reply['questionItemId'])
                    if method == 'request_user_input_async':
                        self.async_answers.add((ident, call, index))
                except (ValueError, KeyError, TypeError):
                    continue
        for token, request in list(self.requests.items()):
            if request['method'] != 'async-user-input' or request['params']['threadId'] != ident:
                continue
            params = request['params']
            remaining = [question for question in params['questions'] if (ident, params['itemId'], int(question['id'])) not in self.async_answers]
            if params.get('turnId') != active or not remaining:
                self.requests.pop(token, None)
            else:
                params['questions'] = remaining
        for row in rows:
            item = row['item']
            if row.get('turnId') != active or not item.get('questions') or item.get('delivery') != 'async':
                continue
            questions = [{'id': str(index), 'question': question['title'],
                          'options': [{'label': option} for option in question.get('options') or []]}
                         for index, question in enumerate(item['questions']) if (ident, item['id'], index) not in self.async_answers]
            if questions:
                token = 'async-' + item['id']
                self.requests[token] = {'id': item['id'], 'method': 'async-user-input',
                    'params': {'threadId': ident, 'turnId': active, 'itemId': item['id'], 'questions': questions}}

    async def history(self, ident, cursor=None, active=None):
        previous = None if cursor else self.histories.get(ident)
        head = previous['head'] if previous else None
        rows, continuation = [], cursor
        for _ in range(40):
            page = await self.call('thread/items/list', {'threadId': ident, 'cursor': continuation,
                                   'sortDirection': 'desc', 'limit': 100, 'deferOutput': {}})
            rows.extend(page.get('data', []))
            continuation = page.get('nextCursor')
            if not continuation or (head and any(row['item']['id'] == head for row in rows)) or (
                not head and sum(public_item(row) is not None for row in rows) >= 20):
                break
        self.sync_questions(ident, rows, active)
        fresh = [item for row in reversed(rows) if (item := public_item(row)) is not None]
        if cursor:
            return fresh, continuation
        known = OrderedDict((item['id'], item) for item in previous['messages']) if previous else OrderedDict()
        for item in fresh:
            known[item['id']] = item
        result = list(known.values())[-80:]
        self.histories[ident] = {'messages': result, 'head': rows[0]['item']['id'] if rows else head,
                                  'cursor': previous['cursor'] if previous else continuation}
        self.histories.move_to_end(ident)
        while len(self.histories) > 8:
            self.histories.popitem(last=False)
        return result, self.histories[ident]['cursor']

    def waiting(self, ident):
        result = []
        for token, request in self.requests.items():
            params, method = request['params'], request['method']
            if params.get('threadId') != ident:
                continue
            if method in ('item/tool/requestUserInput', 'async-user-input'):
                questions = [{'id': question['id'], 'title': question['question'], 'secret': question.get('isSecret', False),
                    'options': [option['label'] for option in question.get('options') or []]}
                    for question in params.get('questions', [])]
                result.append({'id': token, 'kind': 'question', 'questions': questions})
            elif method in APPROVALS:
                kind = APPROVALS[method]
                if kind == 'permissions':
                    details = json.dumps(params.get('permissions', {}), ensure_ascii=False)
                else:
                    details = params.get('command') or params.get('grantRoot') or ''
                result.append({'id': token, 'kind': 'approval', 'title': 'Autorização necessária',
                    'reason': params.get('reason') or 'O agente precisa da sua autorização para continuar.', 'details': details})
            else:
                result.append({'id': token, 'kind': 'unsupported', 'title': 'Esta solicitação precisa ser respondida no Codex do computador.'})
        return result

    async def detail(self, ident, cursor=None):
        ident = thread_id(ident)
        data = await self.call('thread/read', {'threadId': ident, 'includeTurns': False})
        thread = data['thread']
        if thread.get('status', {}).get('type') == 'active' and ident not in self.subscribed:
            await self.call('thread/resume', {'threadId': ident, 'excludeTurns': True})
            self.subscribed.add(ident)
        turns = await self.call('thread/turns/list', {'threadId': ident, 'limit': 1, 'itemsView': 'notLoaded'})
        active = next((turn['id'] for turn in turns.get('data', []) if turn.get('status') == 'inProgress'), None) if thread.get('status', {}).get('type') == 'active' else None
        text, continuation = await self.history(ident, cursor, active)
        pending = self.waiting(ident)
        info = card(thread)
        if pending:
            info['state'] = 'waiting'
        return {'agent': info, 'messages': text,
                'requests': pending, 'cursor': continuation}

    async def prompt(self, ident, text):
        ident = thread_id(ident)
        if not isinstance(text, str) or not text.strip() or len(text) > 32000:
            raise web.HTTPBadRequest(text='Escreva um prompt com até 32 mil caracteres')
        try:
            text.encode('utf-8')
        except UnicodeError:
            raise web.HTTPBadRequest(text='O prompt contém caracteres inválidos')
        async with self.thread_locks.setdefault(ident, asyncio.Lock()):
            data = await self.call('thread/resume', {'threadId': ident, 'excludeTurns': True})
            self.subscribed.add(ident)
            if data['thread'].get('canAcceptDirectInput') is False:
                raise web.HTTPConflict(text='Envie sua orientação ao agente principal desta tarefa')
            turns = await self.call('thread/turns/list', {'threadId': ident, 'limit': 1, 'itemsView': 'notLoaded'})
            active = next((turn for turn in turns.get('data', []) if turn.get('status') == 'inProgress'), None) if data['thread'].get('status', {}).get('type') == 'active' else None
            content = [{'type': 'text', 'text': text}]
            if active:
                await self.call('turn/steer', {'threadId': ident, 'expectedTurnId': active['id'], 'input': content})
            else:
                await self.call('turn/start', {'threadId': ident, 'input': content})
        return {'ok': True}

    async def create(self, cwd):
        if not isinstance(cwd, str) or len(cwd) > 4096 or '\0' in cwd or not os.path.isabs(cwd) or not Path(cwd).is_dir():
            raise web.HTTPBadRequest(text='Escolha uma pasta existente no computador')
        data = await self.call('thread/start', {'cwd': str(Path(cwd).resolve())})
        return card(data['thread'])

    async def reply(self, ident, token, data):
        ident = thread_id(ident)
        request = self.requests.get(token)
        if not request or request['params'].get('threadId') != ident:
            raise web.HTTPConflict(text='Esta solicitação já foi respondida ou não está mais disponível')
        params, method = request['params'], request['method']
        if method in ('item/tool/requestUserInput', 'async-user-input'):
            answers = data.get('answers')
            questions = {question['id'] for question in params.get('questions', [])}
            if not isinstance(answers, dict) or set(answers) != questions or any(
                not isinstance(value, str) or not value.strip() or len(value) > 16000 for value in answers.values()):
                raise web.HTTPBadRequest(text='Responda a todas as perguntas')
            if method == 'async-user-input':
                payload = [{'answer': answers[question['id']], 'question': question['question'],
                            'questionItemId': json.dumps(['request_user_input_async', params['itemId'], int(question['id'])])}
                           for question in params['questions']]
                text = '<send_user_message_question_reply>\n' + json.dumps(payload) + '\n</send_user_message_question_reply>'
                if self.requests.pop(token, None) is None:
                    raise web.HTTPConflict(text='Esta solicitação já foi respondida')
                answered = {(ident, params['itemId'], int(question['id'])) for question in params['questions']}
                self.async_answers.update(answered)
                try:
                    await self.prompt(ident, text)
                except Exception:
                    self.async_answers.difference_update(answered)
                    self.requests[token] = request
                    raise
                return {'ok': True}
            response = {'answers': {key: {'answers': [value]} for key, value in answers.items()}}
        elif method in APPROVALS:
            decision = data.get('decision')
            if decision not in ('accept', 'decline', 'cancel'):
                raise web.HTTPBadRequest(text='Decisão inválida')
            if method == 'item/permissions/requestApproval':
                response = {'permissions': params['permissions'] if decision == 'accept' else {}, 'scope': 'turn'}
            else:
                response = {'decision': decision}
        else:
            raise web.HTTPConflict(text='Responda a esta solicitação no Codex do computador')
        if self.requests.pop(token, None) is None:
            raise web.HTTPConflict(text='Esta solicitação já foi respondida')
        await self.ws.send_json({'id': request['id'], 'result': response})
        return {'ok': True}

    async def close(self):
        if self.ws:
            await self.ws.close()
        if self.reader:
            self.reader.cancel()
            await asyncio.gather(self.reader, return_exceptions=True)
        if self.client:
            await self.client.close()
        self.client = self.ws = self.reader = None
        self.subscribed.clear()
        self.requests.clear()
        self.histories.clear()
        self.async_answers.clear()
        self.question_heads.clear()
