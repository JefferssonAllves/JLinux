import asyncio
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch, AsyncMock
import contextlib
import io

from aiohttp import CookieJar, WSMsgType, web
from aiohttp.test_utils import TestClient, TestServer

SOURCE = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(SOURCE))
spec = importlib.util.spec_from_file_location('connect_server', SOURCE / 'server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)
HEADERS = {'Host': 'localhost:8765', 'Origin': 'http://localhost:8765'}


class ConnectTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='jlinux-connect-test-')
        self.directory = Path(self.temp.name)
        self.home = self.directory / 'home'
        self.home.mkdir()
        self.companion = server.Companion(self.directory, SOURCE / 'web', desktop=lambda: {}, home=self.home)
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
        for path in ['/api/status', '/api/files', '/api/media', '/api/codex/threads', '/api/files/download', '/api/system/status', '/ws/volume', '/ws/codex/00000000-0000-7000-8000-000000000001', '/terminal/', '/terminal/token', '/terminal/ws', '/ws/desktop', '/vendor/novnc/core/rfb.js']:
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
        self.companion.sessions[server.session_id(token)] = time.time() - 1
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

    async def test_codex_text_stream_is_private_and_closes_on_logout(self):
        await self.login()
        ident = '00000000-0000-7000-8000-000000000001'
        self.companion.codex.detail = AsyncMock(return_value={})
        self.companion.codex.subscribed.add(ident)
        ws = await self.client.ws_connect('/ws/codex/' + ident, headers=HEADERS)
        await ws.receive_json(timeout=2)
        self.companion.codex.message_event('item/started', {'threadId': ident, 'item': {
            'id': 'answer', 'type': 'agentMessage', 'phase': 'final_answer', 'text': 'Texto ao vivo'}})
        self.assertEqual((await ws.receive_json(timeout=2))['messages'][0]['text'], 'Texto ao vivo')
        response = await self.client.post('/api/logout', json={}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertIn((await ws.receive(timeout=2)).type, (WSMsgType.CLOSE, WSMsgType.CLOSED))
        await ws.close()
        await asyncio.sleep(.01)
        self.assertNotIn(ident, self.companion.codex.listeners)

    async def test_volume_stream_coalesces_and_rejects_bad_values(self):
        await self.login()
        entered, release = asyncio.Event(), asyncio.Event()
        commands = []
        async def slow(*args):
            commands.append(args)
            if len(commands) == 1:
                entered.set()
                await release.wait()
            return ''
        self.companion.command = slow
        ws = await self.client.ws_connect('/ws/volume', headers=HEADERS)
        await ws.send_json({'level': .1})
        await asyncio.wait_for(entered.wait(), 2)
        for number in range(50, 100):
            await ws.send_json({'level': number / 100})
        await asyncio.sleep(.03)
        release.set()
        self.assertEqual((await ws.receive_json())['level'], .1)
        self.assertEqual((await ws.receive_json())['level'], .99)
        self.assertEqual(len(commands), 2)
        await ws.send_json({'level': -1})
        self.assertIn('error', await ws.receive_json())
        self.assertEqual(len(commands), 2)
        response = await self.client.post('/api/logout', json={}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertIn((await ws.receive()).type, (WSMsgType.CLOSE, WSMsgType.CLOSED))

    async def test_persistent_pairing_and_key_rotation(self):
        response = await self.client.post('/api/login', json={'key': self.companion.key, 'remember': True}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        token = response.cookies[server.COOKIE].value
        self.assertEqual(response.cookies[server.COOKIE]['max-age'], str(365 * 86400))
        saved = self.directory / 'sessions.json'
        self.assertEqual(saved.stat().st_mode & 0o777, 0o600)
        self.assertNotIn(token, saved.read_text())
        restarted = server.Companion(self.directory, SOURCE / 'web', desktop=lambda: {}, home=self.home)
        self.assertIsNone(restarted.sessions[server.session_id(token)])
        with patch.object(server.time, 'time', return_value=time.time()+2*365*86400):
            self.assertTrue(restarted.session_alive(server.session_id(token)))
        with open(self.directory / 'pair.key', 'w') as out:
            out.write('a' * 43)
        rotated = server.Companion(self.directory, SOURCE / 'web', desktop=lambda: {}, home=self.home)
        self.assertEqual(rotated.sessions, {})
        await self.client.post('/api/logout', json={}, headers=HEADERS)
        self.assertEqual(json.loads(saved.read_text())['sessions'], {})

    async def test_file_listing_transfer_and_no_overwrite(self):
        await self.login()
        (self.home / 'Documents').mkdir()
        (self.home / '.private').write_text('hidden')
        response = await self.client.get('/api/files', headers=HEADERS)
        data = await response.json()
        self.assertEqual([item['name'] for item in data['entries']], ['Documents'])
        response = await self.client.get('/api/files?hidden=1', headers=HEADERS)
        self.assertIn('.private', [item['name'] for item in (await response.json())['entries']])
        data = 'Arquivo do iPhone 🦊'.encode() + b'x' * (1024 * 1024)
        response = await self.client.post('/api/files/upload', params={'path': 'Documents', 'name': 'foto 🦊.txt'},
                         data=data, headers=HEADERS)
        self.assertEqual(response.status, 201, await response.text())
        file = self.home / 'Documents' / 'foto 🦊.txt'
        self.assertEqual(file.read_bytes(), data)
        response = await self.client.post('/api/files/upload', params={'path': 'Documents', 'name': file.name},
                         data=b'OVERWRITE', headers=HEADERS)
        self.assertEqual(response.status, 409)
        self.assertEqual(file.read_bytes(), data)
        response = await self.client.get('/api/files/download', params={'path': 'Documents/' + file.name}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertEqual(await response.read(), data)
        self.assertIn('attachment;', response.headers['Content-Disposition'])
        self.assertEqual(list((self.home / 'Documents').glob('.jlinux-upload-*')), [])

    async def test_files_reject_escape_symlinks_special_and_partial_upload(self):
        await self.login()
        outside = self.directory / 'outside'
        outside.write_bytes(b'PRIVATE')
        (self.home / 'link').symlink_to(outside)
        (self.home / 'escape').symlink_to(self.directory, target_is_directory=True)
        os.mkfifo(self.home / 'pipe')
        for path in ['../outside', str(outside), 'link', 'escape/outside', 'pipe']:
            response = await self.client.get('/api/files/download', params={'path': path}, headers=HEADERS)
            self.assertIn(response.status, (400, 403, 404), path)
        response = await self.client.post('/api/files/upload', params={'path': '', 'name': '../outside'}, data=b'BAD', headers=HEADERS)
        self.assertEqual(response.status, 403)
        response = await self.client.post('/api/files/upload', params={'path': 'escape', 'name': 'bad'}, data=b'BAD', headers=HEADERS)
        self.assertIn(response.status, (403, 404))
        response = await self.client.post('/api/files/upload', params={'name': 'link'}, data=b'BAD', headers=HEADERS)
        self.assertEqual(response.status, 409)
        self.assertEqual(outside.read_bytes(), b'PRIVATE')
        import file_transfer
        async def chunks():
            yield b'12345'
            await asyncio.sleep(.01)
            yield b'67890'
        with patch.object(file_transfer, 'MAX_UPLOAD', 8):
            response = await self.client.post('/api/files/upload', params={'name': 'large'}, data=chunks(), headers=HEADERS)
            self.assertEqual(response.status, 413)
        self.assertFalse((self.home / 'large').exists())
        self.assertEqual(list(self.home.glob('.jlinux-upload-*')), [])

    async def test_system_confirmation_bound_expiring_and_single_use(self):
        await self.login()
        actions = []
        async def execute(action):
            actions.append(action)
            return {'ok': True, 'action': action}
        self.companion.system.execute = execute
        response = await self.client.post('/api/system/action', json={'action': 'reboot'}, headers=HEADERS)
        self.assertEqual(response.status, 409)
        response = await self.client.post('/api/system/prepare', json={'action': 'sh;reboot'}, headers=HEADERS)
        self.assertEqual(response.status, 400)
        for action in ('reboot', 'poweroff', 'logout', 'update'):
            response = await self.client.post('/api/system/prepare', json={'action': action}, headers=HEADERS)
            self.assertEqual(response.status, 200)
            prepared = await response.json()
            data = {'action': action, 'confirmation': prepared['confirmation']}
            response = await self.client.post('/api/system/action', json=data, headers=HEADERS)
            self.assertEqual(response.status, 202)
            response = await self.client.post('/api/system/action', json=data, headers=HEADERS)
            self.assertEqual(response.status, 409)
        self.assertEqual(actions, ['reboot', 'poweroff', 'logout', 'update'])
        response = await self.client.post('/api/system/prepare', json={'action': 'reboot'}, headers=HEADERS)
        prepared = await response.json()
        token = next(iter(self.companion.confirmations))
        self.companion.confirmations[token] = ('reboot', prepared['confirmation'], time.time() - 1)
        response = await self.client.post('/api/system/action', json={'action': 'reboot', 'confirmation': prepared['confirmation']}, headers=HEADERS)
        self.assertEqual(response.status, 409)

    async def test_confirmation_cannot_be_used_by_another_device(self):
        response = await self.login()
        first = response.cookies[server.COOKIE].value
        response = await self.client.post('/api/system/prepare', json={'action': 'reboot'}, headers=HEADERS)
        data = {'action': 'reboot', 'confirmation': (await response.json())['confirmation']}
        self.client.session.cookie_jar.clear()
        await self.login()
        response = await self.client.post('/api/system/action', json=data, headers=HEADERS)
        self.assertEqual(response.status, 409)
        self.client.session.cookie_jar.clear()
        self.client.session.cookie_jar.update_cookies({server.COOKIE: first}, response_url=self.client.make_url('/'))
        called = []
        async def execute(action):
            called.append(action)
            return {'ok': True}
        self.companion.system.execute = execute
        response = await self.client.post('/api/system/action', json=data, headers=HEADERS)
        self.assertEqual(response.status, 202)
        self.assertEqual(called, ['reboot'])

    async def test_existing_trusted_device_migrates_and_cookie_renews(self):
        response = await self.login()
        raw = response.cookies[server.COOKIE].value
        ident = server.session_id(raw)
        self.companion.sessions[ident] = time.time() + 29 * 86400
        self.companion.save_sessions()
        restarted = server.Companion(self.directory, SOURCE / 'web', desktop=lambda: {}, home=self.home)
        self.assertIsNone(restarted.sessions[ident])
        self.assertIsNone(json.loads((self.directory/'sessions.json').read_text())['sessions'][ident])
        self.companion.sessions[ident] = None
        response = await self.client.get('/api/status', headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertEqual(response.cookies[server.COOKIE]['max-age'], str(365*86400))
        self.assertTrue(response.cookies[server.COOKIE]['httponly'])
        response = await self.client.post('/api/logout', json={}, headers=HEADERS)
        self.assertEqual(response.status, 200)
        self.assertFalse(server.Companion(self.directory, SOURCE/'web', home=self.home).session_alive(ident))

    async def test_codex_routes_allow_long_prompts_without_raw_rpc(self):
        await self.login()
        mock=AsyncMock(return_value={'ok':True})
        self.companion.codex.prompt=mock
        ident='00000000-0000-7000-8000-000000000001'
        text='x'*10000
        response=await self.client.post(f'/api/codex/threads/{ident}/prompt',json={'text':text},headers=HEADERS)
        self.assertEqual(response.status,200)
        mock.assert_awaited_once_with(ident,text)
        response=await self.client.post(f'/api/codex/threads/{ident}/prompt',json={'text':'x','method':'command/exec'},headers=HEADERS)
        self.assertEqual(response.status,400)
        response=await self.client.post('/api/media/action',json={'action':'next','player':'x','method':'evil'},headers=HEADERS)
        self.assertEqual(response.status,400)

    async def test_capture_falls_back_if_gpu_cannot_start(self):
        self.companion.desktop=lambda:{'WAYLAND_DISPLAY':'fake'}
        calls=[]
        async def spawn(*args,**kwargs):
            calls.append(args)
            if '-g' in args:
                return type('FailedProcess',(),{'returncode':1})()
            (self.directory/'desktop.sock').touch()
            return type('RunningProcess',(),{'returncode':None})()
        with patch.object(server.asyncio,'create_subprocess_exec',side_effect=spawn):
            path=await self.companion.backend('desktop')
        self.assertTrue(path.exists())
        self.assertEqual(len(calls),2)
        self.assertIn('-g',calls[0]);self.assertNotIn('-g',calls[1])
        self.assertIn('60',calls[1])
        self.companion.processes.clear()

    async def test_pairing_qr_is_generated_locally(self):
        import sys
        sys.path.insert(0, str(SOURCE))
        try:
            import cli
            (self.directory / 'host').write_text('test.tail.ts.net')
            with patch.object(cli, 'STATE', self.directory), patch.object(cli, 'tailnet', return_value='test.tail.ts.net'), \
                 patch.object(cli.shutil, 'which', return_value='/usr/bin/qrencode'), \
                 patch.object(cli.subprocess, 'run') as run, patch.object(sys, 'argv', ['jlinux-connect', 'parear']), \
                 contextlib.redirect_stdout(io.StringIO()) as output:
                cli.main()
                run.assert_called_once_with(['qrencode', '-t', 'UTF8', '-m', '2'],
                    input='https://test.tail.ts.net/#' + self.companion.key, text=True, check=True)
                self.assertIn('Escaneie', output.getvalue())
        finally:
            sys.path.pop(0)

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
