#!/usr/bin/env python3
"""Test panel Bluetooth with mock D-Bus CLI/manager; never changes host radios."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait,stop=h['wait_for'],h['stop']
compositor,shell,pointer=[str(Path(a).resolve()) for a in sys.argv[1:]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-bt-') as directory:
    root=Path(directory)
    mock=root/'busctl'
    mock.write_text(f'''#!{sys.executable}
import pathlib,sys,os,time
root=pathlib.Path(os.environ['BT_TEST'])
with (root/'calls').open('a') as out:out.write(' '.join(sys.argv[1:])+'\\n')
if (root/'hang').exists():time.sleep(20)
if (root/'fail').exists():sys.exit(1)
if sys.argv[3]=='get-property':print('b '+((root/'state').read_text() if (root/'state').exists() else 'false'))
else:(root/'state').write_text(sys.argv[-1])
''');mock.chmod(0o755)
    manager=root/'manager';manager.write_text(f'#!{sys.executable}\nfrom pathlib import Path\nPath({str(root/"opened")!r}).touch()\n');manager.chmod(0o755)
    apps=root/'data/applications';apps.mkdir(parents=True)
    (apps/'blueman-manager.desktop').write_text(f'[Desktop Entry]\nType=Application\nName=Bluetooth test\nExec={manager}\n')
    env=dict(os.environ,PATH=directory,BT_TEST=directory,XDG_DATA_HOME=str(root/'data'),XDG_CONFIG_HOME=str(root/'config'),XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
    logfile=root/'server.log'
    def log():return logfile.read_text(errors='replace')
    def calls():return (root/'calls').read_text() if (root/'calls').exists() else ''
    def click(x,y):subprocess.run([pointer,str(round(x)),str(round(y))],env=env,check=True,timeout=4);time.sleep(.2)
    scale=min(1280/1672,668/889);offset=(1280-1672*scale)/2;top=52-52*scale
    def tile(y):click(offset+1400*scale,top+y*scale)
    try:
        with logfile.open('w') as out:
            server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'socket')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client=subprocess.Popen([shell],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(client)
            wait(lambda:'Interface mapeada: jlinux-panel' in log(),processes,'panel')
            click(1240,18)
            wait(lambda:'get-property' in calls(),processes,'query on open')
            time.sleep(.2);tile(170)
            wait(lambda:(root/'state').exists() and (root/'state').read_text()=='true',processes,'power on')
            time.sleep(.2);tile(170)
            wait(lambda:(root/'state').read_text()=='false',processes,'power off')
            (root/'fail').touch();tile(170);time.sleep(.2);assert client.poll() is None
            (root/'fail').unlink();tile(170)
            wait(lambda:(root/'state').read_text()=='true',processes,'retry after failure')
            (root/'hang').touch();tile(170);time.sleep(5.5);(root/'hang').unlink()
            assert client.poll() is None,'timeout killed shell'
            tile(170)
            wait(lambda:(root/'state').read_text()=='false',processes,'retry after timeout')
            tile(234)
            wait(lambda:(root/'opened').exists(),processes,'device manager')
            assert 'Painel de controle fechado.' in log()
            stop(client);processes.remove(client);assert client.returncode==0
            stop(server);processes.remove(server);assert server.returncode==0
    except Exception:
        print(log(),calls(),file=sys.stderr);raise
    finally:
        for process in reversed(processes):stop(process)
print('Bluetooth: query, on/off, failure recovery and device manager passed.')
