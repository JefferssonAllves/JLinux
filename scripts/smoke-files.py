#!/usr/bin/env python3
"""Exercise native file navigation and argv-safe opening with disposable files."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

spec = importlib.util.spec_from_file_location('terminal_helpers', Path(__file__).with_name('smoke-terminal.py'))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
server_binary, files, pointer, wtype = [str(Path(arg).resolve()) for arg in sys.argv[1:5]]
with tempfile.TemporaryDirectory(prefix='jlinux-files-') as directory:
    runtime = Path(directory)
    folder = runtime / 'content'
    folder.mkdir()
    subfolder = folder / 'A-folder'
    subfolder.mkdir()
    inside = subfolder / 'inside.txt'
    inside.write_text('example')
    outside = folder / 'B-name $(touch SENTINEL).txt'
    outside.write_text('example')
    (folder / '.hidden').write_text('example')
    tools = runtime / 'bin'
    tools.mkdir()
    marker = runtime / 'opened.json'
    opener = tools / 'gio'
    opener.write_text('#!/usr/bin/env python3\nimport json, os, sys\nfrom pathlib import Path\nPath(os.environ["JLINUX_FILES_MARKER"]).write_text(json.dumps(sys.argv[1:]))\n')
    opener.chmod(0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS='headless', WLR_RENDERER='pixman',
               WLR_HEADLESS_OUTPUTS='1', JLINUX_TEST_INPUT='1', PATH=str(tools) + ':' + os.environ['PATH'], JLINUX_FILES_MARKER=str(marker))
    logfile = runtime / 'session.log'
    def key(*args):
        subprocess.run([wtype, '-s', '200', *args], env=env, check=True, timeout=5)
    def opened(path):
        helpers.wait_for(lambda: marker.exists() and marker.stat().st_size and json.loads(marker.read_text()) == ['open', str(path)], [server, client], 'arquivo aberto')
    with logfile.open('w') as out:
        server = subprocess.Popen([server_binary], env=env, stdout=out, stderr=subprocess.STDOUT)
        client = None
        try:
            helpers.wait_for(lambda: 'JLinux compositor pronto:' in logfile.read_text(), [server], 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in runtime.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([files, str(folder)], env=env, stdout=out, stderr=subprocess.STDOUT)
            helpers.wait_for(lambda: 'Janela mapeada: Arquivos' in logfile.read_text(), [server, client], 'gerenciador')
            key('-k', 'Return')
            key('-k', 'Return')
            opened(inside)
            subprocess.run([pointer, '120', '116'], env=env, check=True, timeout=4)
            key('-k', 'Down', '-k', 'Return')
            opened(outside)
            assert not (Path.cwd() / 'SENTINEL').exists()
            assert not (folder / 'SENTINEL').exists()
            key('-M', 'ctrl', 'h', '-m', 'ctrl')
            key('-k', 'F5')
            assert client.poll() is None
            helpers.stop(client)
            assert client.returncode == 0
            preview = runtime / 'files.png'
            subprocess.run([files, '--preview', str(preview), str(folder)], env=env, check=True)
            assert preview.stat().st_size > 1000
            helpers.stop(server)
            assert server.returncode == 0
        except Exception:
            print(logfile.read_text(), file=sys.stderr)
            raise
        finally:
            for process in (client, server):
                if process:
                    helpers.stop(process)
print('Arquivos: pastas, navegação por teclado/mouse, abertura sem shell e cleanup passaram.')
