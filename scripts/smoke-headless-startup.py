#!/usr/bin/env python3
"""An empty backend receives a virtual screen; an existing output is preserved."""
import os
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import tempfile

helpers = runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait, stop = helpers['wait_for'], helpers['stop']
compositor, shell = [str(Path(p).resolve()) for p in sys.argv[1:3]]
for output_count in ('0', '1'):
    processes = []
    with tempfile.TemporaryDirectory(prefix='jlinux-headless-start-') as directory:
        root = Path(directory)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless',
                   WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS=output_count)
        log_file = root / 'log'
        def log():
            return log_file.read_text(errors='replace')
        try:
            with log_file.open('w') as out:
                server = subprocess.Popen([compositor], env=env, stdout=out, stderr=out)
                processes.append(server)
                wait(lambda: 'JLinux compositor pronto:' in log(), processes, 'compositor')
                env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
                client = subprocess.Popen([shell], env=env, stdout=out, stderr=out)
                processes.append(client)
                wait(lambda: 'Interface mapeada: jlinux-panel' in log(), processes, 'graphical shell')
                assert ('tela virtual 1920x1080 ativada' in log()) == (output_count == '0')
                png = subprocess.run(['grim', '-'], env=env, check=True, stdout=subprocess.PIPE, timeout=5).stdout
                assert png[:8] == b'\x89PNG\r\n\x1a\n'
                expected = (1920, 1080) if output_count == '0' else (1280, 720)
                assert struct.unpack('>II', png[16:24]) == expected
                assert log().count('Saída ativa:') == 1
        except Exception:
            print(log(), file=sys.stderr)
            raise
        finally:
            for process in reversed(processes):
                stop(process)
