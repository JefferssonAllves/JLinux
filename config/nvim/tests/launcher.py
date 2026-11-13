"""Regression for detached editor surviving the original terminal's hangup."""
import os
from pathlib import Path
import subprocess
import tempfile
import time

with tempfile.TemporaryDirectory(prefix='nvimcode-launch-') as directory:
    root=Path(directory)
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',XDG_STATE_HOME=str(root/'state'))
    with (root/'server.log').open('w') as out:
        server=subprocess.Popen(['/usr/bin/jlinux-compositor'],env=env,stdout=out,stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                sockets=[p for p in root.glob('wayland-*') if p.is_socket()]
                if sockets:break
                assert server.poll() is None
                time.sleep(.03)
            assert sockets,'no Wayland socket'
            env['WAYLAND_DISPLAY']=sockets[0].name
            for index in range(3):
                before=(root/'server.log').read_text().count('Janela mapeada:')
                terminal=subprocess.Popen(['foot','--title=nvimcode-launch-test','-e','bash','--noprofile','--rcfile',str(Path.home()/'.bashrc'),'-ic','nvimcode'],env=env,stdout=out,stderr=subprocess.STDOUT)
                terminal.wait(timeout=6)
                assert terminal.returncode==0,(root/'server.log').read_text()
                deadline=time.monotonic()+6
                while (root/'server.log').read_text().count('Janela mapeada:')<before+2:
                    assert time.monotonic()<deadline,(root/'server.log').read_text()
                    time.sleep(.05)
                time.sleep(.2)
            print('Launcher: 3 detached editor windows mapped after origin terminals exited.')
        finally:
            server.terminate();server.wait(timeout=5)
