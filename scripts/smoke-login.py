#!/usr/bin/env python3
"""Render login in an isolated compositor; preview does not authenticate."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

server_binary, greeter = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='jlinux-login-') as directory:
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1')
    log = Path(directory) / 'compositor.log'
    with log.open('w') as out:
        server = subprocess.Popen([server_binary], env=env, stdout=out, stderr=subprocess.STDOUT)
        client = None
        try:
            deadline = time.monotonic() + 6
            while 'JLinux compositor pronto:' not in log.read_text():
                assert server.poll() is None
                if time.monotonic() > deadline:
                    raise RuntimeError('Compositor timeout')
                time.sleep(.05)
            env['WAYLAND_DISPLAY'] = next(p.name for p in Path(directory).glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([greeter, '--demo'], env=env, stdout=out, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 6
            while 'Janela mapeada: JLinux Login' not in log.read_text():
                assert client.poll() is None, log.read_text()
                if time.monotonic() > deadline:
                    raise RuntimeError('Login did not map')
                time.sleep(.05)
            time.sleep(.15)
            assert client.poll() is None
            client.terminate()
            assert client.wait(timeout=3) == 0
            output = Path(directory) / 'login.png'
            subprocess.run([greeter, '--preview', str(output)], env=env, check=True)
            assert output.stat().st_size > 1000
            no_auth = dict(env)
            no_auth.pop('GREETD_SOCK', None)
            assert subprocess.run([greeter], env=no_auth).returncode == 1
            server.terminate()
            assert server.wait(timeout=3) == 0
        finally:
            for process in (client, server):
                if process and process.poll() is None:
                    process.kill()
                    process.wait()
