import asyncio
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import time
import unittest

from aiohttp import CookieJar, WSMsgType, web
from aiohttp.test_utils import TestClient, TestServer

SOURCE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('connect_server', SOURCE / 'server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)
HEADERS = {'Host': 'localhost:8765', 'Origin': 'http://localhost:8765'}


class ConnectTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='jlinux-connect-test-')
        self.directory = Path(self.temp.name)
        self.companion = server.Companion(self.directory, SOURCE / 'web', desktop=lambda: {})
        self.commands = []
        async def command(*args):
            self.commands.append(args)
            return 'Volume: 0.42 [MUTED]\n'
        self.companion.command = command
        self.client = TestClient(TestServer(self.companion.application()), cookie_jar=CookieJar(unsafe=True))
        await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()
        self.temp.cleanup()

    async def login(self):
        response = await self.client.post('/api/login', json={'key': self.companion.key}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        return response

    async def test_private_key_and_security_headers(self):
        self.assertEqual((self.directory / 'pair.key').stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.directory.stat().st_mode & 0o777, 0o700)
        self.assertEqual(self.companion.key, server.pairing_key(self.directory))
        response = await self.client.get('/', headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertEqual(response.headers['Referrer-Policy'], 'no-referrer')
        self.assertIn("frame-ancestors 'self'", response.headers['Content-Security-Policy'])
        self.assertNotIn(self.companion.key, await response.text())

    async def test_all_privileged_routes_require_login(self):
        for path in ['/api/status', '/terminal/', '/terminal/token', '/terminal/ws', '/ws/desktop', '/vendor/novnc/core/rfb.js']:
            response = await self.client.get(path, headers=HEADERS)
            self.assertEqual(response.status, 401, path)
        response = await self.client.post('/api/volume', json={'level': .5}, headers=HEADERS)
        self.assertEqual(response.status, 401)
        self.assertEqual(self.commands, [])

    async def test_host_origin_and_size_boundaries(self):
        await self.login()
        for headers in [{'Host': 'evil.invalid', 'Origin': 'http://evil.invalid'},
                        {'Host': 'localhost:8765', 'Origin': 'https://evil.invalid'},
                        {'Host': 'localhost:8765'}]:
            response = await self.client.post('/api/volume', json={'level': .5}, headers=headers)
            self.assertEqual(response.status, 403)
        response = await self.client.post('/api/login', json={'key': 'x' * 6000}, headers=HEADERS)
        self.assertEqual(response.status, 413)
        self.assertEqual(self.commands, [])
        with self.assertRaises(Exception) as caught:
            await self.client.ws_connect('/ws/desktop', headers={'Host': 'localhost:8765', 'Origin': 'https://evil.invalid'})
        self.assertEqual(caught.exception.status, 403)

    async def test_login_rate_limit_cookie_and_expiry(self):
        for key in ['wrong', '\ud800', '', 'x' * 43, 'ç' * 43]:
            response = await self.client.post('/api/login', json={'key': key}, headers=HEADERS)
            self.assertEqual(response.status, 401)
        response = await self.client.post('/api/login', json={'key': self.companion.key}, headers=HEADERS)
        self.assertEqual(response.status, 429)
        self.companion.failures.clear()
        response = await self.login()
        self.assertTrue(response.cookies[server.COOKIE]['httponly'])
        self.assertEqual(response.cookies[server.COOKIE]['samesite'], 'Strict')
        token = response.cookies[server.COOKIE].value
        self.companion.sessions[token] = time.monotonic() - 1
        response = await self.client.get('/api/status', headers=HEADERS)
        self.assertEqual(response.status, 401)

    async def test_audio_validation_and_status(self):
        await self.login()
        for payload in [{'level': -1}, {'level': 1.1}, {'level': True}, {'level': float('nan')},
                        {'level': '1;touch /tmp/unsafe'}, {'mute': 'toggle'}, {'level': .2, 'extra': True}, [], None]:
            response = await self.client.post('/api/volume', json=payload, headers=HEADERS)
            self.assertEqual(response.status, 400, str(payload))
        self.assertEqual(self.commands, [])
        response = await self.client.post('/api/volume', json={'level': .65}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertEqual(self.commands[-1], ('wpctl', 'set-volume', '--limit', '1.0', '@DEFAULT_AUDIO_SINK@', '0.65'))
        response = await self.client.post('/api/volume', json={'mute': True}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertEqual(self.commands[-1], ('wpctl', 'set-mute', '@DEFAULT_AUDIO_SINK@', '1'))
        response = await self.client.get('/api/status', headers=HEADERS)
        data = await response.json()
        self.assertEqual(data['volume'], {'level': .42, 'muted': True})
        self.assertFalse(data['desktop'])

    async def test_desktop_binary_bridge_and_logout(self):
        await self.login()
        socket = self.directory / 'mock-vnc.sock'
        async def connected(reader, writer):
            writer.write(b'RFB 003.008\n')
            await writer.drain()
            try:
                data = await reader.read(100)
                if data:
                    writer.write(data)
                    await writer.drain()
                await reader.read()
            finally:
                writer.close()
                await writer.wait_closed()
        async with await asyncio.start_unix_server(connected, path=socket):
            async def backend(kind):
                self.assertEqual(kind, 'desktop')
                return socket
            self.companion.backend = backend
            ws = await self.client.ws_connect('/ws/desktop', headers=HEADERS, protocols=['binary'])
            message = await asyncio.wait_for(ws.receive(), 3)
            self.assertEqual(message.data, b'RFB 003.008\n')
            await ws.send_bytes(b'hello')
            message = await asyncio.wait_for(ws.receive(), 3)
            self.assertEqual(message.data, b'hello')
            response = await self.client.post('/api/logout', json={}, headers=HEADERS)
            self.assertEqual(response.status, 200)
            message = await asyncio.wait_for(ws.receive(), 3)
            self.assertIn(message.type, (WSMsgType.CLOSE, WSMsgType.CLOSED))
            self.assertEqual(self.companion.sessions, {})
            response = await self.client.get('/api/status', headers=HEADERS)
            self.assertEqual(response.status, 401)

    async def test_revocation_during_backend_start(self):
        await self.login()
        entered, resume = asyncio.Event(), asyncio.Event()
        socket = self.directory / 'slow-vnc.sock'
        async def connected(reader, writer):
            await reader.read()
            writer.close()
            await writer.wait_closed()
        async with await asyncio.start_unix_server(connected, path=socket):
            async def backend(kind):
                entered.set()
                await resume.wait()
                return socket
            self.companion.backend = backend
            async def connect():
                return await self.client.ws_connect('/ws/desktop', headers=HEADERS)
            pending = asyncio.create_task(connect())
            await asyncio.wait_for(entered.wait(), 3)
            response = await self.client.post('/api/logout', json={}, headers=HEADERS)
            self.assertEqual(response.status, 200)
            resume.set()
            with self.assertRaises(Exception) as caught:
                await asyncio.wait_for(pending, 3)
            self.assertEqual(caught.exception.status, 401)

    async def test_terminal_real_pty_and_process_cleanup(self):
        await self.login()
        response = await self.client.get('/terminal/', headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertIn('ttyd', (await response.text()).lower())
        self.assertFalse(self.companion.processes['terminal'].returncode)
        response = await self.client.get('/terminal/token', headers=HEADERS)
        token = await response.json()
        ws = await self.client.ws_connect('/terminal/ws', headers=HEADERS, protocols=['tty'])
        await ws.send_str(json.dumps({'AuthToken': token.get('token', ''), 'columns': 80, 'rows': 24}))
        await ws.send_bytes(b"0printf '%s%s\\n' JLINUX_ CONNECT_TEST\r")
        output = b''
        async with asyncio.timeout(8):
            while b'JLINUX_CONNECT_TEST' not in output:
                message = await ws.receive()
                self.assertEqual(message.type, WSMsgType.BINARY)
                output += message.data
        await ws.close()
        process = self.companion.processes['terminal']
        await self.client.close()
        self.assertIsNotNone(process.returncode)


if __name__ == '__main__':
    unittest.main()
