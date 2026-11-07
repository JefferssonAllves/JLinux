#!/usr/bin/env python3
"""Drag to left/right edges; opening and closing other windows must not retile."""
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
compositor, shell, pointer, foot, wtype = [str(Path(a).resolve()) for a in sys.argv[1:]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-snap-') as directory:
    root = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=str(root / 'config'),
               WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1')
    logfile = root / 'server.log'
    def log():
        return logfile.read_text(errors='replace')
    def mouse(*coords):
        subprocess.run([pointer, *map(str, coords)], env=env, check=True, timeout=4)
    def size():
        marker = root / 'size'
        marker.unlink(missing_ok=True)
        subprocess.run([wtype, '-s', '150', f'stty size > {marker}', '-k', 'Return'], env=env, check=True, timeout=4)
        h.wait_for(lambda: marker.exists() and len(marker.read_text().split()) == 2, processes, 'terminal size')
        return tuple(map(int, marker.read_text().split()))
    def open_window(number):
        client = subprocess.Popen([foot, '--config=/dev/null', '--term=xterm-256color', f'--title=Snap-{number}', 'sh'],
                                  env=env, stdout=out, stderr=subprocess.STDOUT)
        processes.append(client)
        pattern = rf'Janela mapeada: Snap-{number} \((\d+)x(\d+) em (\d+),(\d+)\)'
        h.wait_for(lambda: re.search(pattern, log()), processes, 'map')
        return client, tuple(map(int, re.search(pattern, log()).groups()))
    try:
        with logfile.open('w') as out:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(server)
            h.wait_for(lambda: 'JLinux compositor pronto:' in log(), processes, 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            panel = subprocess.Popen([shell], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(panel)
            h.wait_for(lambda: 'Interface mapeada: jlinux-panel' in log(), processes, 'shell')
            one, (width, height, x, y) = open_window(1)
            original = size()
            two, _ = open_window(2)
            # Select the first window in its non-overlapped upper-left region.
            mouse(x + 10, y + 10)
            assert size() == original, 'opening another window resized the first'
            h.stop(two)
            processes.remove(two)
            assert size() == original, 'closing another window resized the first'
            # Hold at the edge: the preview must precede the configure/drop.
            drag = subprocess.Popen([pointer, str(x+160), str(y-18), '0', '240', '--hold'],
                                    env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
            processes.append(drag)
            assert drag.stdout.readline().strip() == 'drag held'
            h.wait_for(lambda: 'Prévia de encaixe: 640x684 em 0,36' in log(), processes, 'snap preview before drop')
            time.sleep(.22)
            assert 'Janela encaixada:' not in log(), 'preview changed geometry before release'
            drag.communicate('\n', timeout=4)
            assert drag.returncode == 0
            processes.remove(drag)
            h.wait_for(lambda: 'Janela encaixada: Snap-1 (634x645 em 3,72)' in log(), processes, 'left half respecting panel without lateral reserve')

            half = size()
            assert half[1] < original[1]
            mouse(220, 54, 1279, 240)
            h.wait_for(lambda: 'Janela encaixada: Snap-1 (634x645 em 643,72)' in log(), processes, 'right half')
            assert size() == half
            mouse(830, 54, 700, 240)
            assert size() == original, 'dragging away did not restore original dimensions'
            # At the top edge, with x away from the sides, no snap/maximize occurs.
            before = log().count('Janela encaixada:')
            mouse(700, 240, 700, 0)
            assert size() == original
            assert log().count('Janela encaixada:') == before, 'top edge snapped a window'
            h.stop(one)
            processes.remove(one)
            if env.get('JLINUX_REDUCED_MOTION') != '1':
                counts = list(map(int, re.findall(r'Fechamento animado: (\d+) buffers', log())))
                assert counts and max(counts) >= 13, 'closing snapshot lost client buffers'
            else:
                assert 'Fechamento animado:' not in log()
            h.stop(panel)
            processes.remove(panel)
            h.stop(server)
            processes.remove(server)
            assert server.returncode == 0
    except Exception:
        print(log(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            h.stop(process)
print('Side snap: manual windows, left/right halves, restore on drag and no top-edge snap passed.')
