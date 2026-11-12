#!/usr/bin/env python3
"""Decode real local videos and recover a killed web process on private Wayland."""
import http.server
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import tempfile
import threading
import urllib.parse

helpers = runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop, wait_for = helpers['stop'], helpers['wait_for']


class Handler(http.server.BaseHTTPRequestHandler):
    events = []
    clips = {}
    crash_requests = 0
    crash_first_request = threading.Event()
    release_crash_first_request = threading.Event()

    def log_message(self, *args):
        pass

    def do_GET(self):
        route = urllib.parse.urlsplit(self.path).path
        Handler.events.append(self.path)
        if route == '/crash-before-load':
            Handler.crash_requests += 1
            if Handler.crash_requests == 1:
                Handler.crash_first_request.set()
                Handler.release_crash_first_request.wait(10)
            body = b"<!doctype html><title>Recovery</title><script>fetch('/recovered')</script>"
            mime = 'text/html; charset=utf-8'
            self.send_response(200)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        if route == '/recovered':
            self.send_response(204)
            self.end_headers()
            return
        if route in self.clips:
            body = self.clips[route]
            mime = 'video/webm' if route.endswith('.webm') else 'video/mp4'
        else:
            clip = '/clip.webm' if route == '/webm' else '/clip.mp4'
            source = f"v.src='{clip}';"
            if route == '/mse':
                source = """const m = new MediaSource(); v.src = URL.createObjectURL(m);
                m.addEventListener('sourceopen', async () => {
                  try {
                    const b = m.addSourceBuffer('video/mp4; codecs="avc1.42c01e, mp4a.40.2"');
                    b.addEventListener('updateend', () => m.endOfStream(), {once:true});
                    b.appendBuffer(await (await fetch('/fragment.mp4')).arrayBuffer());
                  } catch(e) { fetch('/error?detail='+encodeURIComponent(e.message)); }
                }, {once:true});"""
            body = ("""<!doctype html><meta charset='utf-8'><title>Vídeo de teste</title>
            <body><video id='v' autoplay muted playsinline controls width='320'></video>
            <script>let sent=false;
            v.addEventListener('timeupdate',()=>{
              if (!sent && v.currentTime > .4 && v.videoWidth > 0 && v.getVideoPlaybackQuality().totalVideoFrames > 0) {
                sent=true; fetch('/played?route='+encodeURIComponent(location.pathname));
              }
            });
            v.addEventListener('error',()=>fetch('/error?code='+v.error.code));
            """ + source + "</script></body>").encode()
            mime = 'text/html; charset=utf-8'
        self.send_response(200)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def web_processes(root):
    pending, found = [root], []
    while pending:
        pid = pending.pop()
        try:
            pending.extend(map(int, Path(f'/proc/{pid}/task/{pid}/children').read_text().split()))
            if Path(f'/proc/{pid}/comm').read_text().strip() == 'WPEWebProcess':
                found.append(pid)
        except (FileNotFoundError, ProcessLookupError):
            pass
    return found


def main():
    compositor, browser, wtype = [str(Path(arg).resolve()) for arg in sys.argv[1:4]]
    sandbox = subprocess.run(['bwrap', '--unshare-user', '--ro-bind', '/', '/', 'true'], capture_output=True)
    if sandbox.returncode:
        print('SKIP: namespaces indisponíveis; sandbox preservada')
        return 77
    with tempfile.TemporaryDirectory(prefix='jlinux-media-') as directory:
        runtime = Path(directory)
        for name, codec, audio, extra in (
                ('clip.mp4', 'libx264', 'aac', ['-profile:v', 'baseline', '-level:v', '3.0', '-movflags', '+faststart']),
                ('fragment.mp4', 'libx264', 'aac', ['-profile:v', 'baseline', '-level:v', '3.0', '-movflags', 'frag_keyframe+empty_moov+default_base_moof']),
                ('clip.webm', 'libvpx', 'libopus', [])):
            clip = runtime / name
            subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=12',
                            '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-t', '2', '-c:v', codec,
                            '-threads', '1', '-pix_fmt', 'yuv420p', '-c:a', audio, *extra, str(clip)], check=True, timeout=20)
            Handler.clips['/' + name] = clip.read_bytes()
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CACHE_HOME=directory+'/cache',
                   XDG_DATA_HOME=directory+'/data', XDG_CONFIG_HOME=directory+'/config',
                   WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
                   JLINUX_TEST_INPUT='1', LIBGL_ALWAYS_SOFTWARE='1')
        processes = []
        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        logfile = runtime / 'session.log'
        def log():
            return logfile.read_text(errors='replace')
        def keys(*args):
            subprocess.run([wtype, '-s', '100', *args], env=env, check=True, timeout=5)
        try:
            with logfile.open('w') as output:
                display = subprocess.Popen([compositor], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(display)
                wait_for(lambda: 'JLinux compositor pronto:' in log(), processes, 'Wayland privado')
                env['WAYLAND_DISPLAY'] = next(p.name for p in runtime.glob('wayland-*') if p.is_socket())
                base = f'http://127.0.0.1:{server.server_port}'
                client = subprocess.Popen([browser, base+'/crash-before-load'], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(client)
                wait_for(lambda: Handler.crash_first_request.is_set(), processes, 'primeira requisição antes do carregamento')
                victims = web_processes(client.pid)
                assert victims, 'Processo web não encontrado durante o carregamento inicial'
                for pid in victims:
                    os.kill(pid, signal.SIGKILL)
                wait_for(lambda: 'Processo web encerrado:' in log(), processes, 'falha do processo web')
                wait_for(lambda: Handler.crash_requests == 2 and '/recovered' in Handler.events,
                         processes, 'recuperação automática da navegação inicial')
                assert 'repetindo uma vez' in log(), log()
                Handler.release_crash_first_request.set()
                keys('-M', 'ctrl', '-k', 'l', '-m', 'ctrl', base+'/mp4', '-k', 'Return')
                for route in ('/mp4', '/webm', '/mse'):
                    marker = '/played?route=' + urllib.parse.quote(route, safe='')
                    if route != '/mp4':
                        keys('-M', 'ctrl', '-k', 'l', '-m', 'ctrl', base+route, '-k', 'Return')
                    wait_for(lambda: marker in Handler.events, processes, 'frames e avanço de vídeo '+route)
                victims = web_processes(client.pid)
                assert victims, 'Processo web não encontrado'
                for pid in victims:
                    os.kill(pid, signal.SIGKILL)
                wait_for(lambda: 'Processo web encerrado:' in log(), processes, 'falha isolada de página')
                marker = '/played?route=%2Fmse'; before = Handler.events.count(marker)
                keys('-M', 'ctrl', '-k', 'r', '-m', 'ctrl')
                wait_for(lambda: Handler.events.count(marker) > before, processes, 'recuperação por Ctrl+R')
                keys('-M', 'ctrl', '-k', 'q', '-m', 'ctrl')
                assert client.wait(timeout=5) == 0, log()
                missing = dict(env, GST_PLUGIN_SYSTEM_PATH_1_0=directory+'/no-plugins',
                               GST_PLUGIN_PATH_1_0='', GST_REGISTRY_1_0=directory+'/missing-registry')
                probe = subprocess.run([browser, '--probe', base+'/mp4'], env=missing, capture_output=True, timeout=25)
                assert probe.returncode == 0, probe.stderr.decode(errors='replace')
                assert b'm\xc3\xaddia desativada' in probe.stderr, probe.stderr
                assert not any(p.startswith('/error') for p in Handler.events), Handler.events
                stop(display)
                assert display.returncode == 0, log()
            print('MP4 H.264/AAC, WebM VP8/Opus, MSE, recuperação de processo web e falta de plugins passaram.')
        except Exception:
            print(log(), file=sys.stderr)
            print(Handler.events, file=sys.stderr)
            raise
        finally:
            Handler.release_crash_first_request.set()
            for process in reversed(processes):
                stop(process)
            server.shutdown(); server.server_close(); thread.join()
    return 0


if __name__ == '__main__':
    sys.exit(main())
