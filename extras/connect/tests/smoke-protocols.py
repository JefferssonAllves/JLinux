#!/usr/bin/env python3
"""Remote capture/input must stay opt-in; neither test touches the active session."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time


def message(obj, opcode, value):
    return struct.pack('III', obj, 12 << 16 | opcode, value)


for enabled in (False, True):
    with tempfile.TemporaryDirectory(prefix='jlinux-remote-protocols-') as directory:
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=directory,
                   WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
                   JLINUX_TEST_INPUT='0', JLINUX_REMOTE_CONTROL='0')
        if enabled:
            config = Path(directory) / 'jlinux'
            config.mkdir()
            (config / 'connect-enabled').touch()
        with tempfile.TemporaryFile() as log:
            process = subprocess.Popen([sys.argv[1]], env=env, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 5
                while not any(p.is_socket() for p in Path(directory).glob('wayland-*')):
                    assert process.poll() is None and time.monotonic() < deadline, 'No compositor socket'
                    time.sleep(.05)
                path = next(p for p in Path(directory).glob('wayland-*') if p.is_socket())
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(3)
                    client.connect(str(path))
                    client.sendall(message(1, 1, 2) + message(1, 0, 3))
                    pending, globals, done = b'', set(), False
                    while not done:
                        received = client.recv(65536)
                        assert received, 'Connection closed'
                        pending += received
                        while len(pending) >= 8:
                            obj, header = struct.unpack_from('II', pending)
                            size, opcode = header >> 16, header & 65535
                            if len(pending) < size:
                                break
                            body, pending = pending[8:size], pending[size:]
                            if obj == 2 and opcode == 0:
                                _, length = struct.unpack_from('II', body)
                                globals.add(body[8:8 + length - 1].decode())
                            elif obj == 3 and opcode == 0:
                                done = True
                    for protocol in ('zwlr_screencopy_manager_v1', 'zxdg_output_manager_v1',
                                     'zwp_virtual_keyboard_manager_v1', 'zwlr_virtual_pointer_manager_v1'):
                        assert (protocol in globals) == enabled, (enabled, protocol, globals)
            finally:
                process.terminate()
                process.wait(timeout=5)
print('Remote protocols absent by default, present after explicit opt-in')
