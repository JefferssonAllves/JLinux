#!/usr/bin/env python3
"""Oversized client buffer and delayed configure commits must stay bounded."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
h=runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop,wait=h['stop'],h['wait_for']
compositor,fixture,pointer=[str(Path(a).resolve()) for a in sys.argv[1:]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-motion-sync-') as directory:
    root=Path(directory);logfile=root/'server.log';events=root/'client.log'
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
    def log(): return logfile.read_text(errors='replace')
    def mouse(*args): subprocess.run([pointer,*map(str,args)],env=env,check=True,timeout=4)
    try:
        with logfile.open('w') as out,events.open('w') as clientout:
            server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'socket')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client=subprocess.Popen([fixture],env=env,stdin=subprocess.PIPE,stdout=clientout,stderr=out,text=True);processes.append(client)
            client.stdin.write('create 0 -1 0 0 0 0\n');client.stdin.flush()
            wait(lambda:'Janela mapeada: Layout-0' in log(),processes,'map')
            time.sleep(.25)
            before=log().count('Foco por clique: Layout-0')
            # Buffer is 1280x720, but the declared content geometry is 1000x600.
            mouse(1130,200)
            assert log().count('Foco por clique: Layout-0')==before,'content accepts input outside the frame'
            client.stdin.write('delay 0 -1 650 0 0 0\n');client.stdin.flush()
            mouse(200,62,0,240)
            wait(lambda:'Transição de encaixe iniciada' in log(),processes,'snap')
            time.sleep(.30)
            assert 'Transição de encaixe concluída' not in log(),'live content revealed before resized buffer commit'
            wait(lambda:'size 0 634 681' in events.read_text(),processes,'delayed resize commit')
            wait(lambda:'Transição de encaixe concluída' in log(),processes,'handoff after commit')
            before=log().count('Foco por clique: Layout-0')
            mouse(900,200)
            assert log().count('Foco por clique: Layout-0')==before,'snapped content escaped half screen'
            stop(client);processes.remove(client);stop(server);processes.remove(server);assert server.returncode==0
    except Exception:
        print(log(),events.read_text(),file=sys.stderr);raise
    finally:
        for process in reversed(processes):stop(process)
print('Motion sync: oversized content clipped and delayed resize handoff passed.')
