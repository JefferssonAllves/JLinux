#!/usr/bin/env python3
"""JLinux Connect: private, authenticated gateway; terminal and video stay on Unix sockets."""
import asyncio
from collections import deque
import contextlib
import hmac
import hashlib
import json
import math
import os
from pathlib import Path
import re
import secrets
import signal
import time

from aiohttp import ClientSession, UnixConnector, WSMsgType, web
from system_controls import SystemControls, ACTIONS
from file_transfer import FileTransfer
from media_controls import MediaControls
from codex_bridge import CodexBridge

ROOT = Path(__file__).resolve().parent
STATE = Path.home() / '.local/state/jlinux-connect'
COOKIE = 'jlinux_connect'

def session_id(token):
    return hashlib.sha256(token.encode('utf-8', errors='replace')).hexdigest()

PUBLIC = {'/', '/app.js', '/codex.js', '/sw.js', '/style.css', '/icon.svg', '/icon.png', '/manifest.webmanifest', '/api/login'}


def pairing_key(directory):
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(directory, 0o700)
    path = directory / 'pair.key'
    try:
        with open(path, 'x', opener=lambda p, flags: os.open(p, flags, 0o600)) as out:
            out.write(secrets.token_urlsafe(32))
    except FileExistsError:
        pass
    os.chmod(path, 0o600)
    key = path.read_text().strip()
    if not re.fullmatch(r'[A-Za-z0-9_-]{43}', key):
        raise ValueError('Arquivo de chave inválido; restaure ou gere uma nova chave localmente')
    return key


def desktop_environment():
    """The session uses a private Wayland runtime; systemd's runtime differs."""
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            if process.stat().st_uid != os.getuid():
                continue
            command = (process / 'cmdline').read_bytes().split(b'\0')[0].decode()
            if command != '/usr/bin/jlinux-shell':
                continue
            values = dict(item.decode().split('=', 1) for item in
                          (process / 'environ').read_bytes().split(b'\0') if b'=' in item)
            return {key: values[key] for key in ('WAYLAND_DISPLAY', 'XDG_RUNTIME_DIR',
                    'DBUS_SESSION_BUS_ADDRESS', 'LANG', 'XDG_SESSION_ID') if key in values}
        except (OSError, ValueError, UnicodeError):
            continue
    return {}


class Companion:
    def __init__(self, directory=STATE, assets=ROOT / 'web', desktop=desktop_environment, home=None):
        self.directory, self.assets, self.desktop = directory, assets, desktop
        self.key = pairing_key(directory)
        self.sessions = {}
        sessions_file = directory / 'sessions.json'
        if sessions_file.exists():
            try:
                saved = json.loads(sessions_file.read_text())
                if isinstance(saved, dict) and saved.get('key_id') == session_id(self.key) and isinstance(saved.get('sessions'), dict):
                    self.sessions = {key: (None if value is not None and value > time.time() + 8 * 3600 else value) for key, value in saved['sessions'].items()
                        if re.fullmatch(r'[a-f0-9]{64}', key) and (value is None or type(value) in (int, float)
                        and math.isfinite(value) and time.time() < value <= time.time() + 31 * 86400)}
                    if self.sessions != saved['sessions']:
                        self.save_sessions()
            except (ValueError, OSError):
                pass
        self.confirmations = {}
        self.system = SystemControls(desktop)
        self.files = FileTransfer(home or Path.home())
        self.media = MediaControls(desktop)
        self.codex = CodexBridge()
        self.cookie_refreshed = {}
        self.failures = deque(maxlen=5)
        self.sockets = {}
        self.processes = {}
        self.locks = {name: asyncio.Lock() for name in ('terminal', 'desktop', 'volume')}
        self.hosts = {'127.0.0.1:8765', 'localhost:8765'}
        config = directory / 'host'
        if config.exists():
            host = config.read_text().strip()
            if re.fullmatch(r'[a-z0-9.-]+\.ts\.net', host):
                self.hosts.add(host)

    def origin(self, request):
        return ('https://' if request.host.endswith('.ts.net') else 'http://') + request.host

    @web.middleware
    async def guard(self, request, handler):
        if request.host not in self.hosts:
            raise web.HTTPForbidden(text='Host não autorizado')
        origin = request.headers.get('Origin')
        websocket = request.headers.get('Upgrade', '').lower() == 'websocket'
        if ((request.method not in ('GET', 'HEAD') or websocket) and origin != self.origin(request)) or (origin and origin != self.origin(request)):
            raise web.HTTPForbidden(text='Origem não autorizada')
        if request.path not in PUBLIC:
            token = session_id(request.cookies.get(COOKIE, ''))
            if not self.session_alive(token):
                raise web.HTTPUnauthorized(text='Conecte seu iPhone novamente')
            request['session'] = token
        response = await handler(request)
        token = request.get('session')
        if (token and self.session_alive(token) and self.sessions[token] is None and not response.prepared
            and time.monotonic() - self.cookie_refreshed.get(token, -86401) > 86400):
            self.set_cookie(response, request, request.cookies[COOKIE], 365 * 86400)
            self.cookie_refreshed[token] = time.monotonic()
        return response

    def session_alive(self, token):
        return token in self.sessions and (self.sessions[token] is None or self.sessions[token] > time.time())

    def session_timeout(self, token):
        expiry = self.sessions.get(token, 0)
        return None if expiry is None else max(0, expiry - time.time())

    def set_cookie(self, response, request, token, lifetime):
        response.set_cookie(COOKIE, token, httponly=True, secure=request.host.endswith('.ts.net'),
                            samesite='Strict', max_age=lifetime, path='/')

    async def headers(self, request, response):
        response.headers.update({'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
            'Referrer-Policy': 'no-referrer', 'X-Frame-Options': 'SAMEORIGIN',
            'Content-Security-Policy': "default-src 'self'; script-src 'self'" +
                (" 'unsafe-inline' 'wasm-unsafe-eval'" if request.path.startswith('/terminal') else '') +
                "; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'self'; base-uri 'none'; form-action 'self'"})

    def save_sessions(self):
        temporary = self.directory / ('sessions-' + secrets.token_hex(8) + '.tmp')
        try:
            with open(temporary, 'x', opener=lambda path, flags: os.open(path, flags, 0o600)) as output:
                json.dump({'key_id': session_id(self.key), 'sessions': self.sessions}, output)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, self.directory / 'sessions.json')
            directory = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        finally:
            temporary.unlink(missing_ok=True)

    async def login(self, request):
        now = time.time()
        if len(self.failures) == 5 and now - self.failures[0] < 60:
            raise web.HTTPTooManyRequests(text='Aguarde um minuto antes de tentar novamente')
        data = await self.json(request)
        key = data.get('key')
        remember = data.get('remember', False)
        if type(remember) is not bool:
            raise web.HTTPBadRequest(text='Opção de pareamento inválida')
        if not isinstance(key, str) or not re.fullmatch(r'[A-Za-z0-9_-]{43}', key) or not hmac.compare_digest(key, self.key):
            self.failures.append(now)
            raise web.HTTPUnauthorized(text='Chave de conexão inválida')
        self.sessions = {key: expiry for key, expiry in self.sessions.items() if expiry is None or expiry > now}
        if len(self.sessions) >= 8:
            raise web.HTTPTooManyRequests(text='Muitos dispositivos conectados')
        token = secrets.token_urlsafe(32)
        lifetime = 365 * 86400 if remember else 8 * 3600
        self.sessions[session_id(token)] = None if remember else now + lifetime
        self.save_sessions()
        response = web.json_response({'ok': True})
        self.set_cookie(response, request, token, lifetime)
        return response

    async def logout(self, request):
        token = request['session']
        self.sessions.pop(token, None)
        self.save_sessions()
        self.confirmations.pop(token, None)
        for ws in list(self.sockets.get(token, ())):
            await ws.close(code=1000, message=b'Logged out')
        response = web.json_response({'ok': True})
        response.del_cookie(COOKIE, path='/')
        return response

    async def json(self, request):
        try:
            data = await request.json()
            if not isinstance(data, dict):
                raise ValueError()
            return data
        except (ValueError, UnicodeError):
            raise web.HTTPBadRequest(text='JSON inválido')

    async def command(self, *args):
        env = dict(os.environ, LC_ALL='C', PIPEWIRE_RUNTIME_DIR=f'/run/user/{os.getuid()}')
        process = await asyncio.create_subprocess_exec(*args, env=env,
                     stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        try:
            out, _ = await asyncio.wait_for(process.communicate(), 3)
        except (asyncio.TimeoutError, asyncio.CancelledError):
            with contextlib.suppress(ProcessLookupError):
                process.kill()
            await process.wait()
            raise
        if process.returncode:
            raise web.HTTPServiceUnavailable(text='Não foi possível acessar o áudio do JLinux')
        return out.decode(errors='replace')

    async def status(self, request):
        try:
            out = await self.command('wpctl', 'get-volume', '@DEFAULT_AUDIO_SINK@')
            match = re.fullmatch(r'Volume: ([0-9.]+)( \[MUTED\])?\s*', out)
            volume = {'level': min(1, float(match[1])), 'muted': bool(match[2])} if match else None
        except (OSError, asyncio.TimeoutError, web.HTTPServiceUnavailable):
            volume = None
        return web.json_response({'volume': volume, 'desktop': bool(self.desktop()),
                                  'name': 'JLinux', 'version': 1})

    async def set_volume(self, data):
        if set(data) == {'level'}:
            value = data['level']
            if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= 1:
                raise web.HTTPBadRequest(text='Volume deve estar entre 0 e 100%')
            args = ('set-volume', '--limit', '1.0', '@DEFAULT_AUDIO_SINK@', str(value))
        elif set(data) == {'mute'} and type(data['mute']) is bool:
            args = ('set-mute', '@DEFAULT_AUDIO_SINK@', '1' if data['mute'] else '0')
        else:
            raise web.HTTPBadRequest(text='Controle de áudio inválido')
        async with self.locks['volume']:
            try:
                await self.command('wpctl', *args)
            except (OSError, asyncio.TimeoutError):
                raise web.HTTPServiceUnavailable(text='Áudio indisponível')
        return data

    async def volume(self, request):
        await self.set_volume(await self.json(request))
        return web.json_response({'ok': True})

    async def volume_stream(self, request):
        ws = web.WebSocketResponse(heartbeat=30, max_msg_size=4096, compress=False)
        token = request['session']
        await ws.prepare(request)
        self.sockets.setdefault(token, set()).add(ws)
        changed = asyncio.Event()
        pending = None
        async def writer():
            nonlocal pending
            while True:
                await changed.wait()
                changed.clear()
                value, pending = pending, None
                try:
                    await self.set_volume(value)
                    await ws.send_json(value)
                except web.HTTPException as error:
                    await ws.send_json({'error': error.text})
        worker = asyncio.create_task(writer())
        try:
            async with asyncio.timeout(self.session_timeout(token)):
                async for message in ws:
                    if message.type != WSMsgType.TEXT:
                        break
                    try:
                        value = message.json()
                        if not isinstance(value, dict):
                            raise ValueError()
                        pending = value
                        changed.set()
                    except (ValueError, UnicodeError):
                        await ws.send_json({'error': 'Controle de áudio inválido'})
        except TimeoutError:
            pass
        finally:
            worker.cancel()
            await asyncio.gather(worker, return_exceptions=True)
            self.sockets[token].discard(ws)
            if not self.sockets[token]:
                self.sockets.pop(token)
            await ws.close()
        return ws

    async def system_prepare(self, request):
        data = await self.json(request)
        action = data.get('action')
        if set(data) != {'action'} or not isinstance(action, str) or action not in ACTIONS:
            raise web.HTTPBadRequest(text='Ação inválida')
        confirmation = secrets.token_urlsafe(24)
        self.confirmations[request['session']] = (action, confirmation, time.time() + 30)
        title, explanation = ACTIONS[action]
        return web.json_response({'confirmation': confirmation, 'title': title, 'explanation': explanation})

    async def system_action(self, request):
        data = await self.json(request)
        pending = self.confirmations.pop(request['session'], None)
        nonce = data.get('confirmation')
        if (set(data) != {'action', 'confirmation'} or not pending or pending[0] != data['action'] or
            pending[2] <= time.time() or not isinstance(nonce, str) or
            not re.fullmatch(r'[A-Za-z0-9_-]{32}', nonce) or not hmac.compare_digest(pending[1], nonce)):
            raise web.HTTPConflict(text='Confirmação inválida ou expirada. Solicite a ação novamente.')
        try:
            result = await self.system.execute(data['action'])
        except (OSError, asyncio.TimeoutError):
            raise web.HTTPServiceUnavailable(text='Não foi possível iniciar a ação do sistema')
        return web.json_response(result, status=202)

    async def system_status(self, request):
        try:
            result = await self.system.status(logs=True)
        except (OSError, asyncio.TimeoutError):
            raise web.HTTPServiceUnavailable(text='Não foi possível consultar a atualização')
        return web.json_response(result)

    async def media_status(self, request):
        return web.json_response(await self.media.status())

    async def media_action(self, request):
        data = await self.json(request)
        if set(data) != {'player', 'action'}:
            raise web.HTTPBadRequest(text='Controle de mídia inválido')
        return web.json_response(await self.media.action(data['player'], data['action']))

    async def codex_list(self, request):
        cursor, search = request.query.get('cursor'), request.query.get('search', '')
        if len(search) > 200 or cursor and len(cursor) > 4096:
            raise web.HTTPBadRequest(text='Busca inválida')
        return web.json_response(await self.codex.listing(cursor, search))

    async def codex_detail(self, request):
        cursor = request.query.get('cursor')
        if cursor and len(cursor) > 4096:
            raise web.HTTPBadRequest(text='Página inválida')
        data = await self.codex.detail(request.match_info['thread'], cursor)
        body = json.dumps(data, ensure_ascii=True).encode()
        etag = '"' + hashlib.sha256(body).hexdigest() + '"'
        if request.headers.get('If-None-Match') == etag:
            return web.Response(status=304, headers={'ETag': etag})
        return web.Response(body=body, content_type='application/json', headers={'ETag': etag})

    async def codex_stream(self, request):
        ident = request.match_info['thread']
        await self.codex.detail(ident)
        token = request['session']
        if not self.session_alive(token):
            raise web.HTTPUnauthorized(text='Conecte seu iPhone novamente')
        changed = asyncio.Event()
        listeners = self.codex.listeners.setdefault(ident, set())
        ws = web.WebSocketResponse(heartbeat=30, max_msg_size=4096, compress=False)
        await ws.prepare(request)
        listeners.add(changed)
        self.sockets.setdefault(token, set()).add(ws)
        async def writer():
            changed.set()
            while True:
                await changed.wait()
                await asyncio.sleep(.05)
                changed.clear()
                items = [dict(item) for item in self.codex.live.get(ident, {}).values() if item['text'].strip()]
                await ws.send_json({'messages': items})
        worker = asyncio.create_task(writer())
        try:
            async with asyncio.timeout(self.session_timeout(token)):
                async for _ in ws:
                    break
        except TimeoutError:
            pass
        finally:
            worker.cancel()
            listeners.discard(changed)
            if not listeners:
                self.codex.listeners.pop(ident, None)
            self.sockets.get(token, set()).discard(ws)
            await asyncio.gather(worker, return_exceptions=True)
            await ws.close()
        return ws

    async def codex_prompt(self, request):
        data = await self.json(request.clone(client_max_size=65536))
        if set(data) != {'text'}:
            raise web.HTTPBadRequest(text='Prompt inválido')
        return web.json_response(await self.codex.prompt(request.match_info['thread'], data['text']))

    async def codex_create(self, request):
        data = await self.json(request)
        if set(data) != {'cwd'}:
            raise web.HTTPBadRequest(text='Pasta inválida')
        return web.json_response(await self.codex.create(data['cwd']))

    async def codex_reply(self, request):
        data = await self.json(request.clone(client_max_size=65536))
        token = data.pop('request', None)
        if not isinstance(token, str) or len(token) > 100:
            raise web.HTTPBadRequest(text='Solicitação inválida')
        return web.json_response(await self.codex.reply(request.match_info['thread'], token, data))

    async def backend(self, kind):
        async with self.locks[kind]:
            existing = self.processes.get(kind)
            path = self.directory / (kind + '.sock')
            if existing and existing.returncode is None and path.exists():
                return path
            if existing and existing.returncode is None:
                await self.stop(existing)
            path.unlink(missing_ok=True)
            graphical = self.desktop()
            if kind == 'desktop' and not graphical:
                raise web.HTTPServiceUnavailable(text='Entre na sessão gráfica do JLinux primeiro')
            env = dict(os.environ, **graphical)
            if kind == 'terminal':
                args = ['ttyd', '-q', '-i', str(path), '-b', '/terminal', '-W', '-m', '1',
                        '-t', 'fontSize=15', '-t', 'disableLeaveAlert=true',
                        '-t', 'theme={"background":"#0d0c16","foreground":"#eee9ff"}',
                        '-w', str(Path.home()), 'bash', '-l']
            else:
                args = ['wayvnc', '-g', '-e', '-C', '/dev/null', '-u', '-r', '-R', '-f', '60',
                        '-k', 'br', '-n', 'JLinux', str(path)]
            # Retry without GPU if this compositor/driver cannot initialize acceleration.
            for attempt in range(2 if kind == 'desktop' else 1):
                path.unlink(missing_ok=True)
                process = await asyncio.create_subprocess_exec(*args, env=env, start_new_session=True,
                              stdout=asyncio.subprocess.DEVNULL)
                self.processes[kind] = process
                for _ in range(50):
                    if process.returncode is not None:
                        break
                    if path.exists():
                        return path
                    await asyncio.sleep(.05)
                await self.stop(process)
                if kind == 'desktop' and '-g' in args:
                    args.remove('-g')
                else:
                    break
            raise web.HTTPServiceUnavailable(text='Saia e entre na sessão para ativar o controle de tela' if kind == 'desktop' else 'Terminal indisponível')

    async def stop(self, process):
        if process.returncode is None:
            with contextlib.suppress(ProcessLookupError):
                os.killpg(process.pid, signal.SIGTERM)
            try:
                await asyncio.wait_for(process.wait(), 2)
            except asyncio.TimeoutError:
                with contextlib.suppress(ProcessLookupError):
                    os.killpg(process.pid, signal.SIGKILL)
                await process.wait()

    async def bridge(self, request, kind):
        if sum(len(items) for items in self.sockets.values()) >= 4:
            raise web.HTTPTooManyRequests(text='Limite de conexões simultâneas atingido')
        path = await self.backend(kind)
        ws = web.WebSocketResponse(protocols=['tty'] if kind == 'terminal' else ['binary'],
                                   heartbeat=30, max_msg_size=2**20, compress=False)
        # Establish the backend before acknowledging the browser's WebSocket upgrade.
        async with contextlib.AsyncExitStack() as stack:
            if kind == 'terminal':
                client = await stack.enter_async_context(ClientSession(connector=UnixConnector(path=str(path))))
                backend = await stack.enter_async_context(client.ws_connect('http://localhost/terminal/ws', protocols=['tty'], compress=0, max_msg_size=2**20))
                async def upstream():
                    async for message in backend:
                        if message.type == WSMsgType.BINARY:
                            await ws.send_bytes(message.data)
                        elif message.type == WSMsgType.TEXT:
                            await ws.send_str(message.data)
                async def downstream():
                    async for message in ws:
                        if message.type == WSMsgType.BINARY:
                            await backend.send_bytes(message.data)
                        elif message.type == WSMsgType.TEXT:
                            await backend.send_str(message.data)
            else:
                reader, writer = await asyncio.open_unix_connection(path)
                async def upstream():
                    while data := await reader.read(65536):
                        await ws.send_bytes(data)
                async def downstream():
                    async for message in ws:
                        if message.type != WSMsgType.BINARY:
                            break
                        writer.write(message.data)
                        await writer.drain()
                stack.callback(writer.close)
            token = request['session']
            if not self.session_alive(token):
                raise web.HTTPUnauthorized(text='Sessão encerrada')
            await ws.prepare(request)
            self.sockets.setdefault(token, set()).add(ws)
            tasks = [asyncio.create_task(upstream()), asyncio.create_task(downstream())]
            try:
                remaining = self.session_timeout(token)
                done, _ = await asyncio.wait(tasks, timeout=remaining, return_when=asyncio.FIRST_COMPLETED)
                for task in done:
                    task.result()
            finally:
                for task in tasks:
                    task.cancel()
                await asyncio.gather(*tasks, return_exceptions=True)
                self.sockets[token].discard(ws)
                if not self.sockets[token]:
                    self.sockets.pop(token)
                await ws.close()
        return ws

    async def terminal(self, request):
        if request.path == '/terminal/ws':
            return await self.bridge(request, 'terminal')
        if request.path not in ('/terminal/', '/terminal/token'):
            raise web.HTTPNotFound()
        path = await self.backend('terminal')
        async with ClientSession(connector=UnixConnector(path=str(path))) as client:
            async with client.get('http://localhost' + request.path, allow_redirects=False) as upstream:
                data = await upstream.read()
                return web.Response(body=data, status=upstream.status,
                                    headers={'Content-Type': upstream.headers.get('Content-Type', 'text/html')})

    async def screen(self, request):
        return await self.bridge(request, 'desktop')

    async def asset(self, request):
        filename = 'index.html' if request.path == '/' else request.path.lstrip('/')
        return web.FileResponse(self.assets / filename)

    async def shutdown(self, app):
        for connections in list(self.sockets.values()):
            for ws in list(connections):
                await ws.close(code=1001)
    async def lifecycle(self, app):
        yield
        await self.codex.close()
        for process in self.processes.values():
            await self.stop(process)

    def application(self):
        app = web.Application(middlewares=[self.guard], client_max_size=4096)
        app.on_response_prepare.append(self.headers)
        app.on_shutdown.append(self.shutdown)
        app.cleanup_ctx.append(self.lifecycle)
        for path in PUBLIC - {'/api/login'}:
            app.router.add_get(path, self.asset)
        app.router.add_post('/api/login', self.login)
        app.router.add_post('/api/logout', self.logout)
        app.router.add_get('/api/status', self.status)
        app.router.add_get('/api/media', self.media_status)
        app.router.add_post('/api/media/action', self.media_action)
        app.router.add_get('/api/codex/threads', self.codex_list)
        app.router.add_post('/api/codex/threads', self.codex_create)
        app.router.add_get('/api/codex/threads/{thread}', self.codex_detail)
        app.router.add_post('/api/codex/threads/{thread}/prompt', self.codex_prompt)
        app.router.add_post('/api/codex/threads/{thread}/reply', self.codex_reply)
        app.router.add_post('/api/volume', self.volume)
        app.router.add_get('/ws/volume', self.volume_stream)
        app.router.add_get('/ws/codex/{thread}', self.codex_stream)
        app.router.add_post('/api/system/prepare', self.system_prepare)
        app.router.add_post('/api/system/action', self.system_action)
        app.router.add_get('/api/system/status', self.system_status)
        app.router.add_get('/api/files', self.files.listing)
        app.router.add_get('/api/files/download', self.files.download)
        app.router.add_post('/api/files/upload', self.files.upload)
        app.router.add_get('/ws/desktop', self.screen)
        app.router.add_get('/terminal/{tail:.*}', self.terminal)
        app.router.add_static('/vendor/', self.assets / 'vendor', follow_symlinks=False)
        return app


if __name__ == '__main__':
    if os.getuid() == 0:
        raise SystemExit('Execute como seu usuário, nunca como root.')
    os.umask(0o077)
    companion = Companion()
    web.run_app(companion.application(), host='127.0.0.1', port=8765,
                access_log=None, print=None, shutdown_timeout=5)
