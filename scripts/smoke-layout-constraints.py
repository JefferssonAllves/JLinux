#!/usr/bin/env python3
"""Validate size hints and transient placement through the xdg-shell protocol."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

spec = importlib.util.spec_from_file_location('helpers', Path(__file__).with_name('smoke-terminal.py'))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
compositor, fixture, wtype, pointer = [str(Path(a).resolve()) for a in sys.argv[1:]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-constraints-') as directory:
    runtime = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless', WLR_RENDERER='pixman',
               WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1')
    server_log, client_log = runtime / 'server.log', runtime / 'client.log'
    def size(index):
        matches = re.findall(rf'^size {index} (\d+) (\d+)$', client_log.read_text(), re.M)
        return tuple(map(int, matches[-1])) if matches else None
    def active(index):
        matches = re.findall(rf'^active {index} ([01])$', client_log.read_text(), re.M)
        return matches and matches[-1] == '1'
    def expect(index, dimensions):
        h.wait_for(lambda: size(index) == dimensions, processes, f'window {index}: {dimensions}')
    def command(name, index, parent=-1, minw=0, minh=0, maxw=0, maxh=0):
        client.stdin.write(f'{name} {index} {parent} {minw} {minh} {maxw} {maxh}\n')
        client.stdin.flush()
        if name == 'create':
            h.wait_for(lambda: f'Janela mapeada: Layout-{index}' in server_log.read_text(), processes, f'map {index}')
    def key(name, shift=False):
        args = [wtype, '-s', '100', '-M', 'logo']
        if shift:
            args += ['-M', 'shift']
        args += ['-k', name]
        if shift:
            args += ['-m', 'shift']
        subprocess.run(args + ['-m', 'logo'], env=env, check=True, timeout=4)
    try:
        with server_log.open('w') as out, client_log.open('w') as events:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(server)
            h.wait_for(lambda: 'JLinux compositor pronto:' in server_log.read_text(), processes, 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in runtime.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=events, stderr=out, text=True, env=env)
            processes.append(client)
            command('create', 0)
            h.wait_for(lambda: size(0), processes, 'initial size')
            baseline = size(0)
            expect(0, baseline)
            command('create', 1, minw=900)
            expect(0, baseline)
            h.wait_for(lambda: size(1) and size(1)[0] >= 900, processes, 'minimum width preserved')
            subprocess.run([pointer, '160', '90', '1279', '240'], env=env, check=True, timeout=4)
            assert 'Janela encaixada: Layout-1' not in server_log.read_text(), 'minimum-width window was forced into half screen'
            assert size(1)[0] >= 900
            command('create', 2, maxw=300, maxh=200)
            expect(0, baseline)
            expect(2, (300, 200))
            command('create', 3, parent=0, maxw=400, maxh=200)
            expect(3, (400, 200))
            assert 'Layout-3 (400x200 em 340,280)' in server_log.read_text(), 'transient was not centered on its parent'
            expect(0, baseline)
            # Parent is the least recently focused window; Alt+Tab selects it.
            subprocess.run([wtype, '-s', '100', '-M', 'alt', '-k', 'Tab', '-m', 'alt'], env=env, check=True, timeout=4)
            key('2', shift=True)
            key('2')
            expect(0, baseline)
            subprocess.run([wtype, '-s', '100', '-M', 'alt', '-k', 'Tab', '-m', 'alt'], env=env, check=True, timeout=4)
            h.wait_for(lambda: active(3), processes, 'transient followed parent to workspace 2')
            # A new transient of the moved child must inherit workspace 2 too.
            command('create', 4, parent=3, maxw=200, maxh=100)
            expect(4, (200, 100))
            key('1')
            h.wait_for(lambda: not active(4), processes, 'nested transient hidden on workspace switch')
            command('limits', 2, minw=800)
            h.wait_for(lambda: size(2) and size(2)[0] >= 800, processes, 'updated minimum honored')
            key('2')
            expect(0, baseline)
            client.stdin.close()
            client.wait(timeout=4)
            assert client.returncode == 0
            h.stop(server)
            assert server.returncode == 0
        print('xdg-shell: minimum/maximum hints, dynamic constraints, parent transients and workspace inheritance passed.')
    except Exception:
        print(server_log.read_text(), client_log.read_text(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            h.stop(process)
