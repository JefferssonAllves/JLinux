#!/usr/bin/env python3
"""Exercise remaps, fullscreen, abrupt disconnect and shutdown with live ghosts."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

spec = importlib.util.spec_from_file_location('helpers', Path(__file__).with_name('smoke-terminal.py'))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
compositor, fixture, wtype = [str(Path(a).resolve()) for a in sys.argv[1:]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-window-motion-') as directory:
    root = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1')
    reduced = env.get('JLINUX_REDUCED_MOTION') == '1'
    logfile = root / 'server.log'
    client_logfile = root / 'client.log'
    def log():
        return logfile.read_text(errors='replace')
    def client_log():
        return client_logfile.read_text(errors='replace')
    def command(name, number=0):
        client.stdin.write(f'{name} {number} -1 0 0 0 0\n')
        client.stdin.flush()
    try:
        with logfile.open('w') as out, client_logfile.open('w') as events:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(server)
            h.wait_for(lambda: 'JLinux compositor pronto:' in log(), processes, 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([fixture], env=env, stdin=subprocess.PIPE, stdout=events, stderr=out, text=True)
            processes.append(client)
            command('create')
            h.wait_for(lambda: log().count('Janela mapeada: Layout-0') == 1, processes, 'opening')
            h.wait_for(lambda: 'fullscreen 0 0' in client_log(), processes, 'initial window state')
            subprocess.run([wtype, '-k', 'F11'], env=env, check=True, timeout=5)
            h.wait_for(lambda: 'fullscreen 0 1' in client_log() and 'size 0 1280 720' in client_log(), processes, 'F11 fullscreen')
            subprocess.run([wtype, '-k', 'F11'], env=env, check=True, timeout=5)
            h.wait_for(lambda: client_log().count('fullscreen 0 0') >= 2, processes, 'F11 restore')
            time.sleep(.22)
            for count in range(1, 7):
                command('unmap')
                if not reduced:
                    h.wait_for(lambda: log().count('Fechamento animado:') >= count, processes, 'closing')
                else:
                    time.sleep(.03)
                command('remap')
                h.wait_for(lambda: log().count('Janela mapeada: Layout-0') == count+1, processes, 'remapping')
            command('fullscreen')
            time.sleep(.08)
            command('restore')
            time.sleep(.08)
            # Disconnect while opening/closing animations may still be active.
            command('create', 1)
            h.wait_for(lambda: 'Janela mapeada: Layout-1' in log(), processes, 'second window')
            client.kill(); client.wait(timeout=4); processes.remove(client)
            if not reduced:
                h.wait_for(lambda: log().count('Fechamento animado:') >= 8, processes, 'abrupt client exit')
                counts = list(map(int, re.findall(r'Fechamento animado: (\d+) buffers', log())))
                assert max(counts) >= 13, 'snapshot must retain client and decoration buffers'
            else:
                assert 'Fechamento animado:' not in log()
            h.stop(server); processes.remove(server)
            assert server.returncode == 0
            assert 'ERROR: AddressSanitizer' not in log() and 'runtime error:' not in log()
    except Exception:
        print(log(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            h.stop(process)
print('Window transitions: remap, fullscreen, disconnect and shutdown passed.')
