#!/usr/bin/env python3
"""Real Super tap/chord, native launcher hit testing and shell restart in private Wayland."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
stop, wait=h['stop'],h['wait_for']
compositor,shell,wtype,pointer=map(lambda a:str(Path(a).resolve()),sys.argv[1:5])
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-futurista-') as directory:
    root=Path(directory)
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
    logfile=root/'session.log'
    def log():return logfile.read_text(errors='replace')
    def keys(*args):subprocess.run([wtype,'-s','100',*args],env=env,check=True,timeout=5)
    def mouse(x,y):subprocess.run([pointer,str(x),str(y)],env=env,check=True,timeout=5)
    try:
        with logfile.open('w') as out:
            server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            for iteration in range(2):
                client=subprocess.Popen([shell],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(client)
                wait(lambda:log().count('Interface mapeada: jlinux-panel')>iteration,processes,'shell')
                sock=root/'jlinux-shell-control'
                assert sock.stat().st_mode&0o777==0o600
                duplicate=subprocess.run([shell],env=env,stdout=out,stderr=subprocess.STDOUT,timeout=4)
                assert duplicate.returncode!=0 and sock.exists(),'duplicate shell replaced live socket'
                dock_maps=log().count('Interface mapeada: jlinux-radial-dock')
                control_maps=log().count('Interface mapeada: jlinux-control-center')
                opened=log().count('Dock circular aberto.')
                control_opened=log().count('Painel de controle aberto.')
                keys('-M','logo','-k','F13','-m','logo')
                assert log().count('Dock circular aberto.')==opened,'Super chord opened launcher'
                keys('-k','Super_L')
                wait(lambda:log().count('Dock circular aberto.')>opened,processes,'Super alone opens dock')
                wait(lambda:log().count('Interface mapeada: jlinux-radial-dock')>dock_maps,processes,'launcher mapped')
                assert log().count('Painel de controle aberto.')==control_opened,'Super opened control panel'
                keys('-k','Super_L')
                wait(lambda:log().count('Dock circular fechado.')>dock_maps,processes,'Super closes dock')
                keys('-k','Super_L')
                wait(lambda:log().count('Interface mapeada: jlinux-radial-dock')>dock_maps+1,processes,'reopen')
                mapped=log().count('Janela mapeada: JLinux Terminal')
                mouse(round(986*1280/1672),round(263*720/941))
                wait(lambda:log().count('Janela mapeada: JLinux Terminal')>mapped,processes,'radial terminal button launches real Foot')
                keys('-M','logo','-k','q','-m','logo')
                mouse(1240,24)
                wait(lambda:log().count('Painel de controle aberto.')>control_opened,processes,'right button opens control panel')
                wait(lambda:log().count('Interface mapeada: jlinux-control-center')>control_maps,processes,'control surface')
                terminals=log().count('Janela mapeada: JLinux Terminal')
                mouse(round(986*1280/1672),round(263*720/941))
                wait(lambda:log().count('Painel de controle fechado.')>control_maps,processes,'dock region is outside control panel')
                assert log().count('Janela mapeada: JLinux Terminal')==terminals,'hidden dock received click'
                mouse(1240,24)
                wait(lambda:log().count('Interface mapeada: jlinux-control-center')>control_maps+1,processes,'reopen control panel')
                mouse(1240,24)
                wait(lambda:log().count('Painel de controle fechado.')>control_maps+1,processes,'right button closes control panel')
                before_dock=log().count('Dock circular aberto.')
                keys('-k','Super_L')
                wait(lambda:log().count('Dock circular aberto.')>before_dock,processes,'open before mode switch')
                before_control=log().count('Painel de controle aberto.')
                mouse(1240,24)
                wait(lambda:log().count('Painel de controle aberto.')>before_control,processes,'right button switches visible dock to controls')
                before_dock=log().count('Dock circular aberto.')
                keys('-k','Super_L')
                wait(lambda:log().count('Dock circular aberto.')>before_dock,processes,'Super switches controls to dock only')
                before_close=log().count('Dock circular fechado.')
                keys('-k','Super_L')
                wait(lambda:log().count('Dock circular fechado.')>before_close,processes,'close switched dock')
                stop(client);processes.remove(client)
                assert client.returncode==0,log()
                assert not sock.exists(),'stale control socket after shell exit'
            stop(server)
            assert server.returncode==0,log()
    except Exception:
        print(log(),file=sys.stderr);raise
    finally:
        for p in reversed(processes):stop(p)
print('Futurista: Super tap/chord, open/close, terminal launch and socket cleanup passed.')
