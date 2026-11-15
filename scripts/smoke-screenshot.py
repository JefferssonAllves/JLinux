#!/usr/bin/env python3
"""Print copies a real PNG; a failed capture preserves the previous clipboard."""
import os
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import tempfile

helpers = runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait, stop = helpers['wait_for'], helpers['stop']
compositor, wtype, foot = [str(Path(p).resolve()) for p in sys.argv[1:4]]
script = Path(__file__).with_name('jlinux-screenshot').resolve()
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-screenshot-') as directory:
    root = Path(directory)
    commands = root / 'bin'
    commands.mkdir()
    (commands / 'jlinux-screenshot').symlink_to(script)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless',
               WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1',
               PATH=str(commands) + os.pathsep + os.environ['PATH'])
    log_path = root / 'log'
    def log():
        return log_path.read_text(errors='replace')
    def clipboard(kind):
        return subprocess.run(['wl-paste', '--no-newline', '--type', kind], env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=3)
    try:
        with log_path.open('w') as output:
            server = subprocess.Popen([compositor], env=env, stdout=output, stderr=output)
            processes.append(server)
            wait(lambda: 'JLinux compositor pronto:' in log(), processes, 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([foot, '--config=/dev/null', '--title=Screenshot test', 'sh'],
                                      env=env, stdout=output, stderr=output)
            processes.append(client)
            wait(lambda: 'Janela mapeada: Screenshot test' in log(), processes, 'test window')
            subprocess.run([wtype, '-k', 'Print'], env=env, check=True, timeout=4)
            wait(lambda: clipboard('image/png').returncode == 0, processes, 'PNG in clipboard')
            png = clipboard('image/png').stdout
            assert png[:8] == b'\x89PNG\r\n\x1a\n'
            assert struct.unpack('>II', png[16:24]) == (1280, 720)
            wait(lambda: not list(root.glob('jlinux-shot.*')), processes, 'temporary image cleanup')
            subprocess.run(['wl-copy', '--type', 'text/plain'], input=b'preserve-me', env=env, check=True, timeout=4)
            (commands / 'grim').write_text('#!/bin/sh\nexit 1\n')
            (commands / 'grim').chmod(0o755)
            result = subprocess.run([str(script)], env=env, timeout=4)
            assert result.returncode != 0
            assert clipboard('text/plain').stdout == b'preserve-me'
            assert not list(root.glob('jlinux-shot.*'))
    except Exception:
        print(log(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            stop(process)
