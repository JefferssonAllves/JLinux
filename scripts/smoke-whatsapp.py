#!/usr/bin/env python3
"""Verify app identity, private profile, single instance and app shortcuts offline."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
helpers=runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop,wait=helpers['stop'],helpers['wait_for']
compositor,browser,wtype=map(lambda x:str(Path(x).resolve()),sys.argv[1:4])
sandbox=subprocess.run(["bwrap","--unshare-user","--ro-bind","/","/","true"],capture_output=True)
if sandbox.returncode:
    print("SKIP: namespaces indisponíveis; sandbox preservada")
    sys.exit(77)
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-whatsapp-test-') as directory:
    root=Path(directory)
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_DATA_HOME=directory+'/data',
        XDG_CACHE_HOME=directory+'/cache',XDG_CONFIG_HOME=directory+'/config',
        WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',
        JLINUX_TEST_INPUT='1',LIBGL_ALWAYS_SOFTWARE='1',https_proxy='http://127.0.0.1:9')
    logfile=root/'app.log'
    def log(): return logfile.read_text(errors='replace')
    try:
        with logfile.open('w') as out:
            server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            app=subprocess.Popen([browser,'--whatsapp'],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(app)
            profile=root/'data/jlinux/whatsapp'
            wait(lambda:(profile/'instance.lock').exists(),processes,'perfil privado')
            assert profile.stat().st_mode&0o777==0o700
            assert not (root/'data/jlinux/browser').exists(),'App compartilhou perfil do navegador'
            duplicate=subprocess.run([browser,'--whatsapp'],env=env,capture_output=True,timeout=8)
            assert duplicate.returncode!=0 and b'WhatsApp j' in duplicate.stderr,duplicate.stderr
            wait(lambda:'Janela mapeada: WhatsApp' in log(),processes,'janela WhatsApp')
            subprocess.run([wtype,'-M','ctrl','-k','t','-m','ctrl','-s','200','-M','ctrl','-k','w','-m','ctrl'],env=env,check=True,timeout=4)
            assert app.wait(timeout=5)==0,'Ctrl+W não fechou app de janela única'
            processes.remove(app)
            # A stale lock file must not prevent restart after the owner exits.
            app=subprocess.Popen([browser,'--whatsapp'],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(app)
            time.sleep(1)
            assert app.poll() is None,log()
            stop(app);processes.remove(app)
            rejected=subprocess.run([browser,'--whatsapp','file:///etc/passwd'],env=env,capture_output=True,timeout=5)
            assert rejected.returncode==2
            stop(server)
        print('WhatsApp: perfil separado/privado, instância única, reinício e atalhos sem abas passaram; site não autenticado.')
    except Exception:
        print(log(),file=sys.stderr);raise
    finally:
        for process in reversed(processes): stop(process)
