#!/usr/bin/env python3
"""Exercise automatic tiling with real clients in an isolated headless compositor."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

spec = importlib.util.spec_from_file_location('helpers', Path(__file__).with_name('smoke-terminal.py'))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
compositor, pointer, foot, wtype = [str(Path(a).resolve()) for a in sys.argv[1:]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-tiling-') as directory:
    runtime = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless',
               WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1', JLINUX_TILING='1')
    logfile = runtime / 'server.log'
    def log():
        return logfile.read_text(errors='replace')
    def key(name, shift=False):
        args = [wtype, '-s', '100', '-M', 'logo']
        if shift:
            args += ['-M', 'shift']
        args += ['-k', name]
        if shift:
            args += ['-m', 'shift']
        subprocess.run(args + ['-m', 'logo'], env=env, check=True, timeout=4)
    def size():
        marker = runtime / 'size'
        marker.unlink(missing_ok=True)
        subprocess.run([wtype, '-s', '100', f'stty size > {marker}', '-k', 'Return'], env=env, check=True, timeout=4)
        h.wait_for(lambda: marker.exists() and len(marker.read_text().split()) == 2, [processes[0]], 'terminal size')
        return tuple(map(int, marker.read_text().split()))
    def open_terminal(index):
        client = subprocess.Popen([foot, '--config=/dev/null', '--override=key-bindings.fullscreen=F11', '--term=xterm-256color', f'--title=Tile-{index}', 'sh'],
                                  env=env, stdout=out, stderr=subprocess.STDOUT)
        processes.append(client)
        h.wait_for(lambda: f'Janela mapeada: Tile-{index}' in log(), [processes[0], client], 'map')
        return client
    try:
        with logfile.open('w') as out:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(server)
            h.wait_for(lambda: 'JLinux compositor pronto:' in log(), [server], 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in runtime.glob('wayland-*') if p.is_socket())
            one = open_terminal(1)
            single = size()
            two = open_terminal(2)
            side = size()
            key('Up')
            assert size() == single, 'maximize did not occupy workarea'
            key('Down')
            assert size() == side, 'restore did not return to tiling'
            subprocess.run([wtype, '-s', '100', '-k', 'F11'], env=env, check=True, timeout=4)
            full = size()
            assert full[0] > single[0] and full[1] >= single[1], (full, single)
            subprocess.run([wtype, '-s', '100', '-k', 'F11'], env=env, check=True, timeout=4)
            assert size() == side, 'fullscreen restore did not retile'
            assert side[1] < single[1] and side[0] == single[0], (single, side)
            three = open_terminal(3)
            stacked = size()
            assert stacked[0] < side[0] and stacked[1] == side[1], (side, stacked)
            key('Left')
            widened = size()
            assert widened[1] > stacked[1], (stacked, widened)
            key('space')
            promoted = size()
            assert promoted[0] == single[0] and promoted[1] > widened[1], (promoted, widened)
            key('f')
            floating = size()
            key('Right')
            assert size() == floating, 'floating window was retiled'
            key('f')
            key('2', shift=True)
            # The moved window is hidden; another window receives keyboard focus.
            assert size()[0] == single[0], 'workspace move did not restore remaining layout'
            key('2')
            assert size() == single, 'moved window did not fill workspace 2'
            key('1')
            h.stop(two)
            assert size() == single, 'close did not expand remaining window'
            h.stop(one)
            key('2')
            assert size() == single, 'focus was lost across empty workspace'
            h.stop(three)
            h.stop(server)
            assert server.returncode == 0
        print('Automatic tiling: 1/2/3 windows, split, promotion, floating, workspace move/switch, close and focus passed.')
    except Exception:
        print(log(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            h.stop(process)
