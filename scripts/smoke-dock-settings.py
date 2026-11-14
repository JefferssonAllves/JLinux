#!/usr/bin/env python3
"""Click real dock/settings surfaces; application launches use private desktop fixtures."""
import math
import json
import os
from pathlib import Path
import runpy
import socket
import subprocess
import sys
import tempfile
import time
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait,stop=h['wait_for'],h['stop']
compositor,shell,pointer=[str(Path(a).resolve()) for a in sys.argv[1:4]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-dock-settings-') as directory:
    root=Path(directory); logfile=root/'session.log'; calls=root/'calls'
    commands=root/'commands';commands.mkdir()
    apps=root/'data'/'applications';apps.mkdir(parents=True)
    config=root/'config';config.mkdir()
    for name in ('Pictures','Videos'):(root/name).mkdir()
    (config/'user-dirs.dirs').write_text(f'XDG_PICTURES_DIR="{root}/Pictures"\nXDG_VIDEOS_DIR="{root}/Videos"\n')
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_CONFIG_HOME=str(config),XDG_DATA_HOME=str(root/'data'),
             WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1',
             PATH=str(commands)+os.pathsep+os.environ['PATH'],DOCK_TEST=directory)
    mock=commands/'dock-test'
    mock.write_text(f'''#!{sys.executable}
import os,sys,json,pathlib
with pathlib.Path(os.environ['DOCK_TEST'],'calls').open('a') as out:out.write(json.dumps(sys.argv)+'\\n')
''');mock.chmod(0o755)
    (commands/'foot').symlink_to(mock)
    entries={'brave-browser.desktop':'brave','Neovim-Code.desktop':'nvim','org.qbittorrent.qBittorrent.desktop':'torrent',
             'org.jlinux.Files.desktop':'files','org.jlinux.WhatsApp.desktop':'whatsapp',
             'com.github.KRTirtho.Spotube.desktop':'spotube'}
    for entry,name in entries.items():
        (apps/entry).write_text(f'[Desktop Entry]\nType=Application\nName={name}\nExec=dock-test {name} %f\n')
    def log():return logfile.read_text(errors='replace')
    def recorded():return [json.loads(x) for x in calls.read_text().splitlines()] if calls.exists() else []
    scale=min(1280/1672,(720-52)/889);dx=(1280-1672*scale)/2;dy=52+(720-52-889*scale)/2-52*scale
    def click(x,y):subprocess.run([pointer,str(round(dx+x*scale)),str(round(dy+y*scale))],env=env,check=True,timeout=4)
    def dock():
        count=log().count('Dock circular aberto.')
        with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as ctl:ctl.sendto(b'show',str(root/'jlinux-shell-control'))
        wait(lambda:log().count('Dock circular aberto.')>count,processes,'dock')
        time.sleep(.25)
    try:
        with logfile.open('w') as out:
            server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client=subprocess.Popen([shell],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(client)
            wait(lambda:'Interface mapeada: jlinux-panel' in log(),processes,'shell')
            dock()
            positions=tuple((836+250*math.cos(math.radians(-90+i*360/11)),469+250*math.sin(math.radians(-90+i*360/11))) for i in range(11))
            for i in (0,1,2,3,4,8,9,10):
                before=len(recorded());click(*positions[i])
                wait(lambda:len(recorded())>before,processes,f'app {i}')
            data=recorded()
            for name in ('brave','nvim','torrent','files','spotube','whatsapp'):assert any(name in row for row in data),name
            before=len(recorded());click(*positions[6]);click(*positions[7]);time.sleep(.15)
            assert len(recorded())==before,'folder shortcuts duplicated a pending Files launch'
            assert any('btop' in row for row in data),'monitor command'
            click(*positions[5]);wait(lambda:'Configurações abertas.' in log(),processes,'settings gear')
            click(1260,350)
            preference=config/'jlinux'/'shell.ini'
            wait(lambda:preference.exists() and 'animations=false' in preference.read_text(),processes,'animation preference')
            click(1260,550)
            wait(lambda:'neon=false' in preference.read_text(),processes,'neon preference')
            click(450,390);click(1260,350)
            wait(lambda:'tint=false' in preference.read_text(),processes,'appearance preference')
            click(450,480);before=len(recorded());click(900,420)
            time.sleep(.15);assert len(recorded())==before,'system folder action duplicated Files'
            click(900,650)
            wait(lambda:'Interface mapeada: jlinux-power-menu' in log(),processes,'power confirmation')
            # Never confirm the power operation; simply restart the private shell.
            stop(client);processes.remove(client)
            client=subprocess.Popen([shell],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(client)
            wait(lambda:log().count('Interface mapeada: jlinux-panel')==2,processes,'shell restart')
            dock();click(*positions[5]);click(1260,350)
            wait(lambda:'animations=true' in preference.read_text(),processes,'loaded saved preference')
            assert 'neon=false' in preference.read_text() and 'tint=false' in preference.read_text()
            assert preference.stat().st_mode&0o777==0o600
    except Exception:
        print(log(),recorded(),file=sys.stderr);raise
    finally:
        for process in reversed(processes):stop(process)
print('Dock: all launch targets, real folder arguments, categories, persistence and power confirmation passed.')
