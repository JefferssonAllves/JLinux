#!/usr/bin/env python3
"""Run against the installed native desktop: HTML fullscreen and browser chrome restore."""
import http.server
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import threading
h = runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
wait, stop = h['wait_for'], h['stop']
compositor, shell, browser, wtype, pointer = [str(Path(arg).resolve()) for arg in sys.argv[1:6]]
sandbox = subprocess.run(['bwrap', '--unshare-user', '--ro-bind', '/', '/', 'true'], capture_output=True)
if sandbox.returncode:
    print('SKIP: namespaces indisponíveis; sandbox preservada')
    sys.exit(77)
events = []
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        events.append(self.path)
        body = b'''<!doctype html><title>Fullscreen fixture</title><body style="margin:0;background:#268040">
        <script>
        function report(){fetch('/state?w='+innerWidth+'&h='+innerHeight+'&fs='+!!document.fullscreenElement);}
        addEventListener('resize',report);document.addEventListener('fullscreenchange',report);
        addEventListener('keydown',e=>{if(e.key==='f')document.documentElement.requestFullscreen().catch(()=>fetch('/denied'));});
        addEventListener('click',e=>fetch('/click?x='+e.clientX+'&y='+e.clientY));
        addEventListener('keydown',e=>{if(e.key==='x'&&document.fullscreenElement)document.exitFullscreen();});
        report();</script>'''
        self.send_response(200); self.send_header('Content-Type', 'text/html'); self.end_headers()
        self.wfile.write(body)
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-web-fullscreen-') as directory:
    root = Path(directory); logfile = root / 'session.log'
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CACHE_HOME=directory+'/cache',
               XDG_DATA_HOME=directory+'/data', XDG_CONFIG_HOME=directory+'/config',
               WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
               JLINUX_TEST_INPUT='1', LIBGL_ALWAYS_SOFTWARE='1', JLINUX_BROWSER_ADBLOCK='0')
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    def log(): return logfile.read_text(errors='replace')
    def keys(*args): subprocess.run([wtype, '-s', '100', *args], env=env, check=True, timeout=5)
    try:
        with logfile.open('w') as out:
            desktop = subprocess.Popen([compositor], env=env, stdout=out, stderr=subprocess.STDOUT); processes.append(desktop)
            wait(lambda:'JLinux compositor pronto:' in log(), processes, 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            panel = subprocess.Popen([shell], env=env, stdout=out, stderr=subprocess.STDOUT); processes.append(panel)
            wait(lambda:'Interface mapeada: jlinux-panel' in log(), processes, 'panel')
            client = subprocess.Popen([browser, f'http://127.0.0.1:{server.server_port}/'], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(client)
            wait(lambda:any(e.startswith('/state?') for e in events), processes, 'page loaded')
            initial = next(e for e in reversed(events) if e.startswith('/state?'))
            keys('-k', 'Escape', 'f')
            wait(lambda:'/state?w=1280&h=720&fs=true' in events, processes, 'HTML fullscreen 1280x720')
            subprocess.run([pointer, '10', '10'], env=env, check=True, timeout=4)
            wait(lambda:'/click?x=10&y=10' in events, processes, 'page covers top bar and browser header')
            subprocess.run([pointer, '1270', '710'], env=env, check=True, timeout=4)
            wait(lambda:'/click?x=1270&y=710' in events, processes, 'page covers bottom right')
            before = len(events); keys('-k', 'Escape')
            wait(lambda:initial in events[before:], processes, 'Escape restores viewport')
            before = len(events); keys('f')
            wait(lambda:'/state?w=1280&h=720&fs=true' in events[before:], processes, 'second HTML fullscreen')
            before = len(events); keys('x')
            wait(lambda:initial in events[before:], processes, 'DOM exit restores viewport')
            before = len(events); keys('-k', 'F11')
            wait(lambda:'/state?w=1280&h=720&fs=false' in events[before:], processes, 'F11 fullscreen')
            before = len(events); keys('-k', 'Escape')
            wait(lambda:initial in events[before:], processes, 'Escape exits F11')
            # The URL field must work again after the chrome is restored.
            keys('-M', 'ctrl', '-k', 'l', '-m', 'ctrl', f'http://127.0.0.1:{server.server_port}/restored', '-k', 'Return')
            wait(lambda:'/restored' in events, processes, 'restored address input')
            stop(client); processes.remove(client)
    except Exception:
        print(log(), events, file=sys.stderr); raise
    finally:
        for process in reversed(processes): stop(process)
        server.shutdown()
print('Fullscreen: HTML/F11 1280x720, Escape and address restore passed.')
