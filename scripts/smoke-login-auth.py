#!/usr/bin/env python3
"""Mock greetd protocol; no system PAM or real credentials are involved."""
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())

def receive(sock):
    def exact(count):
        data = b''
        while len(data) < count:
            chunk = sock.recv(count - len(data))
            if not chunk:
                raise RuntimeError('Unexpected EOF')
            data += chunk
        return data
    size = struct.unpack('=I', exact(4))[0]
    if not 0 < size < 8192:
        raise RuntimeError('Invalid request size')
    return json.loads(exact(size))

def send(sock, obj):
    data = json.dumps(obj, separators=(',', ':')).encode()
    # Exercise partial framing, not just a single socket write.
    packet = struct.pack('=I', len(data)) + data
    sock.sendall(packet[:2])
    sock.sendall(packet[2:7])
    sock.sendall(packet[7:])

for scenario in ('success', 'denied', 'oversize', 'invalid-json', 'unknown-prompt', 'eof'):
    with tempfile.TemporaryDirectory(prefix='jlinux-auth-') as directory:
        path = str(Path(directory) / 'greetd.sock')
        with socket.socket(socket.AF_UNIX) as server:
            server.bind(path)
            server.listen(1)
            server.settimeout(4)
            client = subprocess.Popen([binary, path])
            try:
                with server.accept()[0] as connection:
                    connection.settimeout(4)
                    assert receive(connection) == {'type': 'create_session', 'username': 'test-user'}
                    if scenario == 'oversize':
                        connection.sendall(struct.pack('=I', 65537))
                    elif scenario == 'invalid-json':
                        connection.sendall(struct.pack('=I', 3) + b'bad')
                    elif scenario == 'eof':
                        connection.shutdown(socket.SHUT_RDWR)
                    elif scenario == 'unknown-prompt':
                        send(connection, {'type': 'auth_message', 'auth_message_type': 'unknown', 'auth_message': 'bad'})
                    elif scenario == 'denied':
                        send(connection, {'type': 'error', 'error_type': 'auth_error', 'description': 'denied'})
                        assert receive(connection) == {'type': 'cancel_session'}
                        send(connection, {'type': 'success'})
                    else:
                        for kind, expected in (('secret', 'fake-test-password'), ('visible', '123456'), ('info', None)):
                            send(connection, {'type': 'auth_message', 'auth_message_type': kind, 'auth_message': 'Test prompt'})
                            assert receive(connection) == {'type': 'post_auth_message_response', 'response': expected}
                        send(connection, {'type': 'success'})
                        request = receive(connection)
                        assert request['type'] == 'start_session'
                        assert request['cmd'] == ['/usr/bin/dbus-run-session', '/usr/bin/jlinux-session', '--drm', '--no-terminal']
                        assert 'XDG_SESSION_TYPE=wayland' in request['env']
                        send(connection, {'type': 'success'})
                    code = client.wait(timeout=4)
                    assert code == (0 if scenario == 'success' else 3 if scenario == 'denied' else 1), (scenario, code)
            finally:
                if client.poll() is None:
                    client.kill()
                    client.wait()
print('greetd IPC: prompts, denial, cancellation, malformed frames and secret clearing passed.')
