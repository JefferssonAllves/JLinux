#!/usr/bin/env python3
"""JLinux Connect: private, authenticated gateway; terminal and video stay on Unix sockets."""
import asyncio
from collections import deque
import contextlib
import hmac
import math
import os
from pathlib import Path
import re
import secrets
import signal
import time

from aiohttp import ClientSession, UnixConnector, WSMsgType, web

ROOT = Path(__file__).resolve().parent
STATE = Path.home() / '.local/state/jlinux-connect'
COOKIE = 'jlinux_connect'
PUBLIC = {'/', '/app.js', '/style.css', '/icon.svg', '/icon.png', '/manifest.webmanifest', '/api/login'}


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
                    'DBUS_SESSION_BUS_ADDRESS', 'LANG') if key in values}
        except (OSError, ValueError, UnicodeError):
            continue
    return {}


class Companion:
    def __init__(self, directory=STATE, assets=ROOT / 'web', desktop=desktop_environment):
        self.directory, self.assets, self.desktop = directory, assets, desktop
        self.key = pairing_key(directory)
        self.sessions = {}
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
            token = request.cookies.get(COOKIE, '')
            if self.sessions.get(token, 0) <= time.monotonic():
                raise web.HTTPUnauthorized(text='Conecte seu iPhone novamente')
            request['session'] = token
        return await handler(request)

    async def headers(self, request, response):
        response.headers.update({'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
            'Referrer-Policy': 'no-referrer', 'X-Frame-Options': 'SAMEORIGIN',
            'Content-Security-Policy': "default-src 'self'; script-src 'self'" +
                (" 'unsafe-inline' 'wasm-unsafe-eval'" if request.path.startswith('/terminal') else '') +
                "; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'self'; base-uri 'none'; form-action 'self'"})

    async def login(self, request):
        now = time.monotonic()
        if len(self.failures) == 5 and now - self.failures[0] < 60:
            raise web.HTTPTooManyRequests(text='Aguarde um minuto antes de tentar novamente')
        data = await self.json(request)
        key = data.get('key')
        if not isinstance(key, str) or not re.fullmatch(r'[A-Za-z0-9_-]{43}', key) or not hmac.compare_digest(key, self.key):
            self.failures.append(now)
            raise web.HTTPUnauthorized(text='Chave de conexão inválida')
        self.sessions = {key: expiry for key, expiry in self.sessions.items() if expiry > now}
        if len(self.sessions) >= 8:
            raise web.HTTPTooManyRequests(text='Muitos dispositivos conectados')
        token = secrets.token_urlsafe(32)
        self.sessions[token] = now + 8 * 3600
        response = web.json_response({'ok': True})
        response.set_cookie(COOKIE, token, httponly=True, secure=request.host.endswith('.ts.net'),
                            samesite='Strict', max_age=8 * 3600, path='/')
        return response

    async def logout(self, request):
        token = request['session']
        self.sessions.pop(token, None)
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

    async def volume(self, request):
        data = await self.json(request)
        async with self.locks['volume']:
            if set(data) == {'level'}:
                value = data['level']
                if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= 1:
                    raise web.HTTPBadRequest(text='Volume deve estar entre 0 e 100%')
                args = ('set-volume', '--limit', '1.0', '@DEFAULT_AUDIO_SINK@', str(value))
            elif set(data) == {'mute'} and type(data['mute']) is bool:
                args = ('set-mute', '@DEFAULT_AUDIO_SINK@', '1' if data['mute'] else '0')
            else:
                raise web.HTTPBadRequest(text='Controle de áudio inválido')
            try:
                await self.command('wpctl', *args)
            except (OSError, asyncio.TimeoutError):
                raise web.HTTPServiceUnavailable(text='Áudio indisponível')
        return web.json_response({'ok': True})

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
                args = ['wayvnc', '-e', '-C', '/dev/null', '-u', '-r', '-R', '-f', '20',
                        '-k', 'br', '-n', 'JLinux', str(path)]
            # Each backend is private to this Unix user; no unauthenticated TCP ports.
            process = await asyncio.create_subprocess_exec(*args, env=env, start_new_session=True,
                          stdout=asyncio.subprocess.DEVNULL)
            self.processes[kind] = process
            for _ in range(50):
                if process.returncode is not None:
                    raise web.HTTPServiceUnavailable(text='Saia e entre na sessão para ativar o controle de tela' if kind == 'desktop' else 'Terminal indisponível')
                if path.exists():
                    return path
                await asyncio.sleep(.05)
            await self.stop(process)
            raise web.HTTPServiceUnavailable(text='O serviço demorou a iniciar')

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
            if self.sessions.get(token, 0) <= time.monotonic():
                raise web.HTTPUnauthorized(text='Sessão encerrada')
            await ws.prepare(request)
            self.sockets.setdefault(token, set()).add(ws)
            tasks = [asyncio.create_task(upstream()), asyncio.create_task(downstream())]
            try:
                remaining = max(0, self.sessions[token] - time.monotonic())
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
        app.router.add_post('/api/volume', self.volume)
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
