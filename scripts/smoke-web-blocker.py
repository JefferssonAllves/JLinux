#!/usr/bin/env python3
"""Verify native blocking before download, cosmetics and compiled-cache reuse on local HTTP."""
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    events = []
    ad_bytes = 0

    def log_message(self, *args):
        pass

    def do_GET(self):
        Handler.events.append(self.path)
        if self.path.startswith('/pagead/'):
            body = b'window.adLoaded=true;' + b' ' * (512 * 1024)
            mime = 'application/javascript'
            Handler.ad_bytes += len(body)
        elif self.path == '/normal.js':
            body, mime = b'window.normalLoaded=true;', 'application/javascript'
        else:
            body = b'''<!doctype html><meta charset="utf-8"><title>Blocker fixture</title>
            <body><ins class="adsbygoogle">advert</ins><div class="ads">normal content</div>
            <script src="/pagead/banner.js"></script><script src="/normal.js"></script>
            <script>let x=new XMLHttpRequest();
            x.open('GET','/result?ad='+!!window.adLoaded+'&normal='+!!window.normalLoaded+
              '&hidden='+(getComputedStyle(document.querySelector('ins')).display==='none')+
              '&content='+(getComputedStyle(document.querySelector('.ads')).display!=='none'),false);
            if(location.pathname==='/') x.send();</script></body>'''
            mime = 'text/html; charset=utf-8'
        self.send_response(200)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    binary = str(Path(sys.argv[1]).resolve())
    sandbox = subprocess.run(['bwrap', '--unshare-user', '--ro-bind', '/', '/', 'true'], capture_output=True)
    if sandbox.returncode:
        print('SKIP: namespaces indisponíveis; sandbox preservada')
        return 77
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='jlinux-blocker-') as directory:
            env = dict(os.environ, XDG_CACHE_HOME=directory, LIBGL_ALWAYS_SOFTWARE='1')
            for enabled in (True, True, False):
                Handler.events.clear(); Handler.ad_bytes = 0
                env['JLINUX_BROWSER_ADBLOCK'] = '1' if enabled else '0'
                run = subprocess.run([binary, '--probe', f'http://127.0.0.1:{server.server_port}/'],
                                     env=env, capture_output=True, text=True, timeout=25)
                assert run.returncode == 0, run.stderr
                expected = f'/result?ad={str(not enabled).lower()}&normal=true&hidden={str(enabled).lower()}&content=true'
                assert expected in Handler.events, (Handler.events, run.stderr)
                assert ('/pagead/banner.js' in Handler.events) != enabled, Handler.events
                assert '/normal.js' in Handler.events, Handler.events
                measurement = next(json.loads(line) for line in run.stdout.splitlines() if line.startswith('{"engine"'))
                results.append(dict(enabled=enabled, advertisement_bytes=Handler.ad_bytes, **measurement))
                if enabled:
                    cache = list((Path(directory) / 'jlinux/browser/filters').glob('*'))
                    assert cache, 'Compiled filter was not cached'
                    if len(results) == 1:
                        cached_files = {p.name: p.stat().st_mtime_ns for p in cache}
                    else:
                        assert cached_files == {p.name: p.stat().st_mtime_ns for p in cache}, 'Warm start recompiled the filter'
            # An unusable cache must disable blocking visibly, without stopping navigation.
            broken = Path(directory) / 'broken'
            broken.mkdir(); (broken / 'jlinux').write_text('not a directory')
            env.update(XDG_CACHE_HOME=str(broken), JLINUX_BROWSER_ADBLOCK='1')
            Handler.events.clear()
            run = subprocess.run([binary, '--probe', f'http://127.0.0.1:{server.server_port}/'],
                                 env=env, capture_output=True, text=True, timeout=25)
            assert run.returncode == 0, run.stderr
            assert 'Bloqueio de anúncios indisponível:' in run.stderr, run.stderr
            assert '/result?ad=true&normal=true&hidden=false&content=true' in Handler.events, Handler.events
            print(json.dumps(results))
            print('Blocker: no ad download, normal content preserved, cosmetic rule, disable, cache reuse and cache failure passed.')
    finally:
        server.shutdown(); server.server_close(); thread.join()
    return 0


if __name__ == '__main__':
    sys.exit(main())
