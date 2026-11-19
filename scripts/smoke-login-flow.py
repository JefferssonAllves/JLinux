#!/usr/bin/env python3
"""Type through the real login UI against a fake greetd; no PAM is invoked."""
import importlib.util
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading

spec = importlib.util.spec_from_file_location('terminal_helpers', Path(__file__).with_name('smoke-terminal.py'))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
server_binary, greeter, wtype = [str(Path(arg).resolve()) for arg in sys.argv[1:4]]
with tempfile.TemporaryDirectory(prefix='jlinux-login-flow-') as directory:
    runtime = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless', WLR_RENDERER='pixman',
               WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1', GREETD_SOCK=str(runtime / 'auth.sock'))
    prompted = threading.Event()
    errors = []
    def worker(server):
        try:
            with server.accept()[0] as connection:
                connection.settimeout(6)
                def receive():
                    def exact(length):
                        data = b''
                        while len(data) < length:
                            chunk = connection.recv(length - len(data))
                            if not chunk:
                                raise RuntimeError('Login closed IPC early')
                            data += chunk
                        return data
                    length = struct.unpack('=I', exact(4))[0]
                    assert 0 < length < 8192
                    return json.loads(exact(length))
                def send(obj):
                    data = json.dumps(obj, separators=(',', ':')).encode()
                    connection.sendall(struct.pack('=I', len(data)) + data)
                assert receive() == {'type': 'create_session', 'username': 'test-user'}
                send({'type': 'auth_message', 'auth_message_type': 'secret', 'auth_message': 'Password:'})
                prompted.set()
                assert receive() == {'type': 'post_auth_message_response', 'response': 'fake-login-secret'}
                send({'type': 'success'})
                request = receive()
                assert request['type'] == 'start_session'
                assert request['cmd'] == ['/usr/bin/dbus-run-session', '/usr/bin/jlinux-session', '--drm', '--no-terminal']
                send({'type': 'success'})
        except BaseException as error:
            errors.append(error)
            prompted.set()
    with socket.socket(socket.AF_UNIX) as ipc, (runtime / 'session.log').open('w') as out:
        ipc.bind(env['GREETD_SOCK'])
        ipc.listen(1)
        ipc.settimeout(8)
        thread = threading.Thread(target=worker, args=(ipc,), daemon=True)
        thread.start()
        server = subprocess.Popen([server_binary], env=env, stdout=out, stderr=subprocess.STDOUT)
        client = None
        try:
            log = runtime / 'session.log'
            helpers.wait_for(lambda: 'JLinux compositor pronto:' in log.read_text(), [server], 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in runtime.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([greeter], env=env, stdout=out, stderr=subprocess.STDOUT)
            helpers.wait_for(lambda: 'Janela mapeada: JLinux Login' in log.read_text(), [server, client], 'login')
            subprocess.run([wtype, '-s', '200', 'test-user', '-k', 'Return'], env=env, check=True, timeout=5)
            assert prompted.wait(5), 'No auth prompt after username'
            if errors:
                raise errors[0]
            subprocess.run([wtype, '-s', '200', 'fake-login-secret', '-k', 'Return'], env=env, check=True, timeout=5)
            assert client.wait(timeout=5) == 0
            thread.join(timeout=5)
            assert not thread.is_alive()
            if errors:
                raise errors[0]
            assert 'fake-login-secret' not in log.read_text()
            helpers.stop(server)
            assert server.returncode == 0
        except Exception:
            print(log.read_text(), file=sys.stderr)
            raise
        finally:
            for process in (client, server):
                if process:
                    helpers.stop(process)
print('Login UI: typed username/secret, session request and credential-free logs passed.')
