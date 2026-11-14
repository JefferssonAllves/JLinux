#!/usr/bin/env python3
"""Verify the legacy decoration preference received by GTK clients."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time


def message(obj, opcode, payload=b''):
    return struct.pack('II', obj, ((8 + len(payload)) << 16) | opcode) + payload


def string(value):
    data = value.encode() + b'\0'
    return struct.pack('I', len(data)) + data + b'\0' * (-len(data) % 4)


with tempfile.TemporaryDirectory(prefix='jlinux-decoration-') as directory:
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless',
               WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1')
    with tempfile.TemporaryFile() as log:
        server = subprocess.Popen([sys.argv[1]], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while not list(Path(directory).glob('wayland-*')):
                assert server.poll() is None, 'Compositor exited before creating its socket'
                assert time.monotonic() < deadline, 'No Wayland socket'
                time.sleep(.05)
            path = next(p for p in Path(directory).glob('wayland-*') if p.is_socket())
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(5)
                client.connect(str(path))
                client.sendall(message(1, 1, struct.pack('I', 2)))  # get_registry
                pending = b''
                bound = False
                while True:
                    received = client.recv(65536)
                    assert received, 'Wayland connection closed before decoration preference'
                    pending += received
                    while len(pending) >= 8:
                        obj, header = struct.unpack_from('II', pending)
                        size, opcode = header >> 16, header & 65535
                        if len(pending) < size:
                            break
                        body, pending = pending[8:size], pending[size:]
                        if obj == 2 and opcode == 0:
                            name, length = struct.unpack_from('II', body)
                            interface = body[8:8 + length - 1].decode()
                            if interface == 'org_kde_kwin_server_decoration_manager':
                                client.sendall(message(2, 0, struct.pack('I', name) +
                                                       string(interface) + struct.pack('II', 1, 3)))
                                bound = True
                        elif obj == 3 and opcode == 0:
                            assert bound and struct.unpack('I', body)[0] == 2, 'Expected server-side decorations'
                            print('GTK receives server-side decoration preference')
                            sys.exit(0)
                        elif obj == 1 and opcode == 0:
                            raise AssertionError(f'Wayland protocol error: {body!r}')
        finally:
            server.terminate()
            server.wait(timeout=5)
