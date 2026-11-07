#!/usr/bin/env python3
"""Click volume controls against a fake wpctl; host audio is never modified."""
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
helpers = runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait_for, stop = helpers['wait_for'], helpers['stop']
compositor, shell, pointer = [str(Path(p).resolve()) for p in sys.argv[1:4]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-volume-') as directory:
    root = Path(directory)
    root.chmod(0o700)
    mock = root / 'wpctl'
    calls = root / 'calls'
    mock.write_text(f'''#!{sys.executable}
import json, os, pathlib, sys, time
root = pathlib.Path(os.environ['VOLUME_TEST'])
with (root / 'calls').open('a') as out:
    out.write(json.dumps(sys.argv[1:]) + '\\n')
if (root / 'hang').exists(): time.sleep(20)
if (root / 'fail').exists(): sys.exit(1)
if sys.argv[1] == 'get-volume': print('Volume: 0.40 [MUTED]')
''')
    mock.chmod(0o755)
    env = dict(os.environ, PATH=directory, VOLUME_TEST=directory, XDG_RUNTIME_DIR=directory,
               WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1')
    server_log, shell_log = root / 'server.log', root / 'shell.log'
    def log(): return server_log.read_text(errors='replace')
    def recorded(): return [json.loads(x) for x in calls.read_text().splitlines()] if calls.exists() else []
    def click(x, y):
        subprocess.run([pointer, str(x), str(y)], env=env, check=True, timeout=3)
        time.sleep(.18)
    try:
        with server_log.open('w') as server_out, shell_log.open('w') as shell_out:
            server = subprocess.Popen([compositor], env=env, stdout=server_out, stderr=subprocess.STDOUT)
            processes.append(server)
            wait_for(lambda: 'JLinux compositor pronto:' in log(), [server], 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            for live in (False, True):
                client = subprocess.Popen([shell] + (['--volume-panel'] if live else []), env=env,
                                          stdout=shell_out, stderr=subprocess.STDOUT)
                processes.append(client)
                name = 'jlinux-volume-button' if live else 'jlinux-panel'
                wait_for(lambda: f'Interface mapeada: {name}' in log(), [server, client], name)
                time.sleep(.2)
                if not live:
                    click(1074, 18)
                    assert 'Interface mapeada: jlinux-volume\n' not in log(), 'Área do volume removida ainda abre o popup'
                    stop(client)
                    assert client.returncode == 0
                    continue
                click(974, 18)
                wait_for(lambda: recorded() and recorded()[-1][0] == 'get-volume', [server, client], 'consulta')
                time.sleep(.15)
                for x, expected in ((950, ['set-volume', '--limit', '1.0', '@DEFAULT_AUDIO_SINK@', '5%-']),
                                    (1180, ['set-volume', '--limit', '1.0', '@DEFAULT_AUDIO_SINK@', '5%+']),
                                    (1050, ['set-mute', '@DEFAULT_AUDIO_SINK@', 'toggle'])):
                    before = len(recorded())
                    click(x, 140)
                    wait_for(lambda: len(recorded()) >= before + 2, [server, client], 'ação e nova consulta')
                    assert recorded()[before] == expected, recorded()
                (root / 'fail').touch()
                click(1050, 103)
                time.sleep(.2)
                assert client.poll() is None, 'Falha do áudio derrubou shell'
                (root / 'fail').unlink()
                (root / 'hang').touch()
                click(1050, 103)
                time.sleep(3.3)
                (root / 'hang').unlink()
                # The timeout must release the pending command so retry works.
                before = len(recorded())
                click(1050, 103)
                wait_for(lambda: len(recorded()) > before, [server, client], 'recuperação após timeout')
                click(950, 188)
                stop(client)
                assert client.returncode == 0
            stop(server)
            assert server.returncode == 0
        print('Volume: consulta, ±5%, mute, falha, timeout e recuperação em shell/botão isolado; backend simulado.')
    except Exception:
        print(log(), shell_log.read_text(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes): stop(process)
