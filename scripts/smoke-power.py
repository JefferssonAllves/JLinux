#!/usr/bin/env python3
"""Exercise power confirmation with mock commands; never call real logind actions."""
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
with tempfile.TemporaryDirectory(prefix='jlinux-power-test-') as directory:
    root = Path(directory)
    root.chmod(0o700)
    commands = root / 'commands'
    commands.mkdir()
    calls = root / 'calls'
    failure = root / 'fail'
    mock = f'''#!{sys.executable}
import json, os, pathlib, sys
with open(os.environ['POWER_CALLS'], 'a') as out:
    out.write(json.dumps([pathlib.Path(sys.argv[0]).name, *sys.argv[1:]]) + '\\n')
sys.exit(1 if pathlib.Path(os.environ['POWER_FAIL']).exists() else 0)
'''
    for command in ('systemctl', 'loginctl'):
        path = commands / command
        path.write_text(mock)
        path.chmod(0o755)
    # PATH is intentionally ONLY the mocks: no test failure can run host commands.
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless',
               WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1',
               XDG_SESSION_ID='test_session', PATH=str(commands),
               POWER_CALLS=str(calls), POWER_FAIL=str(failure))
    server_log, client_log = root / 'server.log', root / 'shell.log'
    def log():
        return server_log.read_text(errors='replace')
    def recorded():
        return [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else []
    def click(x, y):
        subprocess.run([pointer, str(x), str(y)], env=env, check=True, timeout=3)
        time.sleep(.12)
    try:
        with server_log.open('w') as server_out, client_log.open('w') as client_out:
            server = subprocess.Popen([compositor], env=env, stdout=server_out, stderr=subprocess.STDOUT)
            processes.append(server)
            wait_for(lambda: 'JLinux compositor pronto:' in log(), [server], 'socket')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            maps = 0
            for panel_only in (False, True):
                args = [shell] + (['--power-panel'] if panel_only else [])
                client = subprocess.Popen(args, env=env, stdout=client_out, stderr=subprocess.STDOUT)
                processes.append(client)
                name = 'jlinux-power-button' if panel_only else 'jlinux-panel'
                wait_for(lambda: f'Interface mapeada: {name}' in log(), [server, client], name)
                time.sleep(.15)
                for action, label in enumerate(('logout', 'reboot', 'poweroff')):
                    before = len(recorded())
                    click(1240, 24)
                    if not panel_only:
                        wait_for(lambda: 'jlinux-control-center' in log(), [server, client], 'central de controle')
                        click(round((1284+action*145)*1280/1672), round(623*720/941))
                    maps += 1
                    wait_for(lambda: log().count('Interface mapeada: jlinux-power-menu') >= maps,
                             [server, client], 'menu de energia')
                    if panel_only: click(1000, 52 + 65 + action * 42)
                    assert len(recorded()) == before, 'Ação antes da confirmação'
                    click(1000, 52 + 240)  # Cancel confirmation.
                    assert len(recorded()) == before, 'Cancelar executou ação'
                    click(1240, 24)
                    if not panel_only:
                        wait_for(lambda: 'jlinux-control-center' in log(), [server, client], 'central de controle')
                        click(round((1284+action*145)*1280/1672), round(623*720/941))
                    maps += 1
                    wait_for(lambda: log().count('Interface mapeada: jlinux-power-menu') >= maps,
                             [server, client], 'reabertura')
                    if panel_only: click(1000, 52 + 65 + action * 42)
                    if action == 2:
                        failure.touch()
                    click(1000, 52 + 180)
                    wait_for(lambda: len(recorded()) > before, [server, client], label)
                    expected = (['loginctl', '--no-ask-password', 'terminate-session', 'test_session']
                                if action == 0 else ['systemctl', '--no-ask-password',
                                                        '--check-inhibitors=yes', label])
                    assert recorded()[-1] == expected, recorded()[-1]
                    time.sleep(.15)
                    assert client.poll() is None
                    if action == 2:
                        wait_for(lambda: 'Ação de sessão recusada' in client_log.read_text(),
                                 [server, client], 'recusa visível e logada')
                        failure.unlink()
                        click(1000, 52 + 240)
                stop(client)
                assert client.returncode == 0
            stop(server)
            assert server.returncode == 0
        print('Energia: três ações, confirmação, cancelamento e recusa, em shell e botão isolado; comandos simulados.')
    except Exception:
        print(server_log.read_text(), client_log.read_text(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            stop(process)
