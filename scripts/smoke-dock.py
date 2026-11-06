#!/usr/bin/env python3
"""Pin, launch, persist and remove a disposable desktop app through the native UI."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

helpers = runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait_for, stop = helpers['wait_for'], helpers['stop']
compositor, shell, pointer = [str(Path(p).resolve()) for p in sys.argv[1:4]]
with tempfile.TemporaryDirectory(prefix='jlinux-dock-test-') as directory:
    root = Path(directory)
    apps = root / 'data/applications'
    apps.mkdir(parents=True)
    config = root / 'config'
    marker = root / 'opened'
    launcher = root / 'launch'
    launcher.write_text(f'#!{sys.executable}\nimport pathlib, signal\nassert signal.SIGTERM not in signal.pthread_sigmask(signal.SIG_BLOCK, [])\npathlib.Path({str(marker)!r}).touch()\n')
    launcher.chmod(0o700)
    (apps / 'test-dock.desktop').write_text(f'[Desktop Entry]\nType=Application\nName=AAAA Dock Test\nExec={launcher}\nTerminal=false\n')
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_DATA_HOME=str(root / 'data'),
               XDG_DATA_DIRS=str(root / 'empty'), XDG_CONFIG_HOME=str(config), HOME=directory,
               WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1')
    settings = config / 'jlinux/dock.ini'
    server = client = None
    logfile = root / 'server.log'
    def click(x, y):
        subprocess.run([pointer, str(x), str(y)], env=env, check=True, timeout=4)
    with logfile.open('w') as out, (root / 'shell.log').open('w') as shell_out:
        try:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT)
            wait_for(lambda: 'JLinux compositor pronto:' in logfile.read_text(), [server], 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            for iteration in range(2):
                client = subprocess.Popen([shell, *sys.argv[4:]], env=env, stdout=shell_out, stderr=subprocess.STDOUT)
                wait_for(lambda: logfile.read_text().count('Interface mapeada: jlinux-dock\n') > iteration,
                         [server, client], 'dock')
                if iteration:
                    marker.unlink()
                    click(34, 475)
                    wait_for(marker.exists, [server, client], 'quarto atalho persistido visível sem rolar')
                click(34, 515)  # Plus below the four visible slots.
                wait_for(lambda: logfile.read_text().count('Interface mapeada: jlinux-dock-apps') > iteration,
                         [server, client], 'lista de aplicativos')
                if not iteration:
                    click(150, 235)  # First row of the centered 380x420 chooser.
                    wait_for(lambda: settings.exists() and 'test-dock.desktop' in settings.read_text(), [server, client], 'preferência salva')
                    assert settings.stat().st_mode & 0o777 == 0o600
                    click(150, 545)  # Close list.
                    click(34, 475)  # Fourth slot; Files, Terminal and Codex remain visible.
                    wait_for(marker.exists, [server, client], 'aplicativo fixado aberto')
                else:
                    assert 'test-dock.desktop' in settings.read_text()
                    click(150, 235)
                    wait_for(lambda: 'test-dock.desktop' not in settings.read_text(), [server, client], 'aplicativo removido')
                stop(client)
                assert client.returncode == 0
            # Invalid settings are preserved, not silently overwritten.
            settings.write_text('invalid configuration')
            client = subprocess.Popen([shell, *sys.argv[4:]], env=env, stdout=shell_out, stderr=subprocess.STDOUT)
            wait_for(lambda: logfile.read_text().count('Interface mapeada: jlinux-dock\n') > 2, [server, client], 'fallback de configuração')
            click(34, 515)
            wait_for(lambda: logfile.read_text().count('Interface mapeada: jlinux-dock-apps') > 2, [server, client], 'lista em fallback')
            click(150, 235)
            assert settings.read_text() == 'invalid configuration'
            stop(client)
            assert client.returncode == 0
        except Exception:
            print(logfile.read_text(), file=sys.stderr)
            print((root / 'shell.log').read_text(), file=sys.stderr)
            raise
        finally:
            for process in (client, server):
                if process:
                    stop(process)
print('Dock: fixar, abrir, persistir, remover e preservar configuração inválida passaram.')
