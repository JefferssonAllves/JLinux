#!/usr/bin/env python3
"""Verify persistent login state across browser restarts in a private Wayland/profile."""
import http.server
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import tempfile
import threading

helpers=runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop,wait=helpers['stop'],helpers['wait_for']
class Handler(http.server.BaseHTTPRequestHandler):
    events=[]
    def log_message(self,*args): pass
    def do_GET(self):
        self.events.append((self.path,self.headers.get('Cookie','')))
        seed=self.path=='/seed'
        script="""const seed=SEED;
        if(seed)localStorage.setItem('login','fixture');
        const open=indexedDB.open('login-fixture',1);
        open.onupgradeneeded=()=>open.result.createObjectStore('auth');
        open.onsuccess=()=>{const db=open.result,tx=db.transaction('auth',seed?'readwrite':'readonly');
          if(seed){tx.objectStore('auth').put('fixture','token');tx.oncomplete=()=>{db.close();fetch('/saved')};}
          else{const read=tx.objectStore('auth').get('token');read.onsuccess=()=>fetch('/restored?local='+
            (localStorage.getItem('login')||'missing')+'&idb='+(read.result||'missing'));tx.oncomplete=()=>db.close();}
        };""".replace('SEED','true' if seed else 'false')
        body=('<!doctype html><title>Login fixture</title><body>profile<script>'+script+'</script></body>').encode()
        self.send_response(200)
        if seed:self.send_header('Set-Cookie','login=fixture; Max-Age=86400; Path=/; HttpOnly; SameSite=Lax')
        self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(body)))
        self.end_headers();self.wfile.write(body)

compositor,browser=map(lambda p:str(Path(p).resolve()),sys.argv[1:3])
sandbox=subprocess.run(["bwrap","--unshare-user","--ro-bind","/","/","true"],capture_output=True)
if sandbox.returncode:
    print("SKIP: namespaces indisponíveis; sandbox preservada")
    sys.exit(77)
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-profile-') as directory:
    root=Path(directory)
    env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_DATA_HOME=directory+'/data',
        XDG_CACHE_HOME=directory+'/cache',XDG_CONFIG_HOME=directory+'/config',
        WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1',
        LIBGL_ALWAYS_SOFTWARE='1',JLINUX_BROWSER_ADBLOCK='0')
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    base=f'http://127.0.0.1:{server.server_port}'
    logfile=root/'browser.log'
    def log():return logfile.read_text(errors='replace')
    def visit(environment,path,report):
        before=len(Handler.events)
        mapped=log().count('Janela mapeada:')
        client=subprocess.Popen([browser,base+path],env=environment,stdout=output,stderr=subprocess.STDOUT)
        processes.append(client)
        wait(lambda:any(p==report for p,c in Handler.events[before:]),[display,client],report)
        wait(lambda:log().count('Janela mapeada:')>mapped,[display,client],'window focus')
        cookie=next(c for p,c in Handler.events[before:] if p==report)
        client.terminate()  # SIGTERM uses the browser's normal main-loop shutdown.
        assert client.wait(timeout=5)==0,log();processes.remove(client)
        return cookie
    try:
        with logfile.open('w') as output:
            display=subprocess.Popen([compositor],env=env,stdout=output,stderr=subprocess.STDOUT);processes.append(display)
            wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
            env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
            assert 'login=fixture' in visit(env,'/seed','/saved')
            profile=root/'data/jlinux/browser'
            assert profile.stat().st_mode&0o777==0o700
            assert (profile/'cookies.sqlite').is_file()
            shutil.rmtree(root/'cache')
            assert 'login=fixture' in visit(env,'/restore','/restored?local=fixture&idb=fixture')
            isolated=dict(env,XDG_DATA_HOME=directory+'/isolated-data',XDG_CACHE_HOME=directory+'/isolated-cache')
            assert not visit(isolated,'/restore','/restored?local=missing&idb=missing')
            before=len(Handler.events)
            probe=subprocess.run([browser,'--probe',base+'/probe'],env=env,capture_output=True,timeout=25)
            assert probe.returncode==0,probe.stderr.decode()
            assert not next(c for p,c in Handler.events[before:] if p=='/probe'),'probe reused persistent cookies'
            # A profile path redirected to another directory must fail without modifying it.
            bad=root/'bad-data/jlinux';bad.mkdir(parents=True)
            target=root/'untouched';target.mkdir(mode=0o755)
            (bad/'browser').symlink_to(target,target_is_directory=True)
            failure=subprocess.run([browser],env=dict(env,XDG_DATA_HOME=str(root/'bad-data')),capture_output=True,timeout=10)
            assert failure.returncode!=0 and b'perfil privado' in failure.stderr
            assert target.stat().st_mode&0o777==0o755
            stop(display)
    except Exception:
        print(log(),file=sys.stderr);raise
    finally:
        for p in reversed(processes):stop(p)
        server.shutdown();server.server_close();thread.join()
print('Profile: cookies HttpOnly, localStorage and IndexedDB survived restart/cache removal; isolation and private permissions passed.')
