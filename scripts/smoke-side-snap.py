#!/usr/bin/env python3
"""Drag to left/right edges; opening and closing other windows must not retile."""
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
            h.wait_for(lambda: 'Interface mapeada: jlinux-dock' in log(), processes, 'shell')
            one, (width, height, x, y) = open_window(1)
            original = size()
            two, _ = open_window(2)
            # Select the first window in its non-overlapped upper-left region.
            mouse(x + 10, y + 10)
            assert size() == original, 'opening another window resized the first'
            h.stop(two)
            processes.remove(two)
            assert size() == original, 'closing another window resized the first'
            mouse(x + 160, y - 18, 0, 240)
            h.wait_for(lambda: 'Janela encaixada: Snap-1 (599x631 em 73,86)' in log(), processes, 'left half respecting dock/panel')
            half = size()
            assert half[1] < original[1]
            mouse(220, 68, 1279, 240)
            h.wait_for(lambda: 'Janela encaixada: Snap-1 (599x631 em 678,86)' in log(), processes, 'right half')
            assert size() == half
            mouse(830, 68, 700, 240)
            assert size() == original, 'dragging away did not restore original dimensions'
            # At the top edge, with x away from the sides, no snap/maximize occurs.
            before = log().count('Janela encaixada:')
            mouse(700, 240, 700, 0)
            assert size() == original
            assert log().count('Janela encaixada:') == before, 'top edge snapped a window'
            h.stop(one)
            processes.remove(one)
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
