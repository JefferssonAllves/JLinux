#!/usr/bin/env python3
"""Real Super hold/release, native launcher hit testing and shell restart in private Wayland."""
import socket
import shutil
import time
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
stop, wait=h['stop'],h['wait_for']
compositor,shell,wtype,pointer=map(lambda a:str(Path(a).resolve()),sys.argv[1:5])
foot=shutil.which("foot")
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-futurista-') as directory:
    root=Path(directory)
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_CONFIG_HOME=directory+'/config',XDG_DATA_HOME=directory+'/data',WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
    commands=root/'commands';commands.mkdir()
    whatsapp=commands/'jlinux-whatsapp'
    whatsapp.write_text(f"#!{sys.executable}\nfrom pathlib import Path\nimport os\nPath(os.environ['XDG_RUNTIME_DIR'],'whatsapp-opened').write_text('opened')\nos.execv({foot!r},[{foot!r},'--config=/dev/null','--app-id=org.jlinux.WhatsApp','--title=WhatsApp fixture','sh'])\n")
    whatsapp.chmod(0o755)
    applications=root/'data'/'applications';applications.mkdir(parents=True)
    (applications/'org.jlinux.WhatsApp.desktop').write_text('[Desktop Entry]\nType=Application\nName=WhatsApp\nExec=jlinux-whatsapp\nIcon=org.jlinux.WhatsApp\n')
    env['PATH']=str(commands)+os.pathsep+os.environ['PATH']
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
                key='Super_L' if iteration==0 else 'Super_R'
                opened=log().count('Dock circular aberto.')
                closed=log().count('Dock circular fechado.')
                control=log().count('Painel de controle aberto.')
                held=subprocess.Popen([wtype,'-P',key,'-s','1200','-p',key],env=env)
                processes.append(held)
                wait(lambda:log().count('Dock circular aberto.')>opened,processes,'press shows dock')
                assert held.poll() is None,'dock opened only after release'
                assert log().count('Painel de controle aberto.')==control,'hold opened control panel'
                held.wait(timeout=4);processes.remove(held)
                wait(lambda:log().count('Dock circular fechado.')>closed,processes,'release hides dock')
                opened=log().count('Dock circular aberto.')
                held=subprocess.Popen([wtype,'-P',key,'-s','2400','-p',key],env=env)
                processes.append(held)
                wait(lambda:log().count('Dock circular aberto.')>opened,processes,'hold for mouse selection')
                mapped=log().count('Janela mapeada: JLinux Terminal')
                if iteration==0:
                    # This point is beyond the old dock bounding box. Hover must select Foot;
                    # clicking the center then confirms that selection rather than the click angle.
                    subprocess.run([pointer,'940','90','--motion'],env=env,check=True,timeout=4)
                    mouse(640,365)
                else:
                    mouse(940,90)  # Direct click in the same angular sector outside the dock.
                wait(lambda:log().count('Janela mapeada: JLinux Terminal')>mapped,processes,'angular hover/outer click opens Foot')
                held.wait(timeout=4);processes.remove(held)
                keys('-M','logo','-k','q','-m','logo')
                mapped=log().count('Janela mapeada: JLinux Terminal')
                keys('-M','logo','-k','Return','-m','logo')
                wait(lambda:log().count('Janela mapeada: JLinux Terminal')>mapped,processes,'Super+Enter still opens Foot')
                wait(lambda:log().count('Dock circular fechado.')==log().count('Dock circular aberto.'),processes,'shortcut leaves no dock')
                keys('-M','logo','-k','q','-m','logo')
                mouse(1240,24)
                wait(lambda:log().count('Painel de controle aberto.')>control,processes,'right button opens controls')
                time.sleep(.25)  # Wait for the control panel's entrance animation.
                control_closed=log().count('Painel de controle fechado.')
                mouse(1240,24)
                wait(lambda:log().count('Painel de controle fechado.')>control_closed,processes,'right button closes controls')
                # Reverse an unfinished close, then move across sectors without clicks.
                with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as ctl:
                    opened=log().count('Dock circular aberto.')
                    ctl.sendto(b'show',str(sock))
                    wait(lambda:log().count('Dock circular aberto.')>opened,processes,'animation open')
                    ctl.sendto(b'hide',str(sock));time.sleep(.04);ctl.sendto(b'show',str(sock))
                    time.sleep(.25)
                    for x,y in ((836,216),(994,270),(1077,407)):
                        subprocess.run([pointer,str(round(x*1280/1672)),str(round(y*720/941)),'--motion'],
                                       env=env,check=True,timeout=4)
                    assert client.poll() is None,'hover/reversal killed shell'
                    closed=log().count('Dock circular fechado.')
                    ctl.sendto(b'hide',str(sock))
                    wait(lambda:log().count('Dock circular fechado.')>closed,processes,'reversed animation closes')
                with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as ctl:
                    opened=log().count('Dock circular aberto.')
                    ctl.sendto(b'show',str(sock))
                    wait(lambda:log().count('Dock circular aberto.')>opened,processes,'WhatsApp dock item')
                    time.sleep(.25)
                    marker=root/'whatsapp-opened'
                    focused=log().count('Aplicativo existente focado: WhatsApp')
                    mouse(round(674*1280/1672),round(277*720/941))
                    if iteration==0:
                        wait(marker.exists,processes,'WhatsApp command launched from former Steam slot')
                        wait(lambda:'Janela mapeada: WhatsApp fixture' in log(),processes,'WhatsApp fixture window')
                    else:
                        wait(lambda:log().count('Aplicativo existente focado: WhatsApp')>focused,processes,'WhatsApp reused after shell restart')
                        assert log().count('Janela mapeada: WhatsApp fixture')==1,'WhatsApp duplicated'
                    closed=log().count('Dock circular fechado.')
                    ctl.sendto(b'hide',str(sock))
                    wait(lambda:log().count('Dock circular fechado.')>closed,processes,'WhatsApp dock closes')
                opened=log().count('Dock circular aberto.')
                closed=log().count('Dock circular fechado.')
                held=subprocess.Popen([wtype,'-P',key,'-s','10000'],env=env)
                processes.append(held)
                wait(lambda:log().count('Dock circular aberto.')>opened,processes,'hold before disconnect')
                stop(held);processes.remove(held)
                wait(lambda:log().count('Dock circular fechado.')>closed,processes,'keyboard disconnect hides dock')
                stop(client);processes.remove(client)
                assert client.returncode==0,log()
                assert not sock.exists(),'stale control socket after shell exit'
            stop(server)
            assert server.returncode==0,log()
    except Exception:
        print(log(),file=sys.stderr);raise
    finally:
        for p in reversed(processes):stop(p)
print('Futurista: Super hold/release, open/close, terminal launch and socket cleanup passed.')
