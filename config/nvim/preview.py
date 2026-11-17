#!/usr/bin/env python3
"""Local HTML preview with reloads for files actually requested by the browser."""
import http.server
import io
from pathlib import Path
import re
import subprocess
import sys
import threading
import urllib.parse


def signature(path):
    try:
        info = path.stat()
        return info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns
    except OSError:
        return None


class Preview(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, file):
        self.root = file.parent.resolve()
        self.files = {file: signature(file)}
        self.version = 0
        self.changed = threading.Condition()
        self.closed = threading.Event()
        self.watcher = threading.Thread(target=self.watch, daemon=True)
        super().__init__(('127.0.0.1', 0), Handler)
        self.watcher.start()

    def watch(self):
        while not self.closed.wait(.4):
            with self.changed:
                modified = False
                for path, old in list(self.files.items()):
                    new = signature(path)
                    if new != old:
                        self.files[path] = new
                        modified = True
                if modified:
                    self.version += 1
                    self.changed.notify_all()

    def server_close(self):
        self.closed.set()
        with self.changed:
            self.changed.notify_all()
        if self.watcher.is_alive():
            self.watcher.join()
        super().server_close()


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(args[2].root), **kwargs)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        super().end_headers()

    def do_GET(self):
        if self.headers.get('Host') != f'127.0.0.1:{self.server.server_port}':
            self.send_error(403)
            return
        if urllib.parse.urlsplit(self.path).path == '/__nvim_live_events':
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.end_headers()
            previous = None
            try:
                while not self.server.closed.is_set():
                    with self.server.changed:
                        if previous == self.server.version:
                            self.server.changed.wait(10)
                        version = self.server.version
                    if self.server.closed.is_set():
                        return
                    self.wfile.write(f'data: {version}\n\n'.encode())
                    self.wfile.flush()
                    previous = version
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        super().do_GET()

    def send_head(self):
        if self.headers.get('Host') != f'127.0.0.1:{self.server.server_port}':
            self.send_error(403)
            return None
        path = Path(self.translate_path(self.path)).resolve()
        if path.is_dir():
            index = next((path/name for name in ('index.html', 'index.htm') if (path/name).is_file()), None)
            if index is None:
                self.send_error(404)
                return None
            path = index.resolve()
        if not path.is_relative_to(self.server.root):
            self.send_error(403)
            return
        if path.is_file():
            with self.server.changed:
                self.server.files.setdefault(path, signature(path))
        if path.suffix.lower() in ('.html', '.htm') and path.is_file():
            try:
                content = path.read_bytes()
            except OSError:
                self.send_error(404)
                return
            script = ("<script>(()=>{const version='" + str(self.server.version) + "';"
                "const events=new EventSource('/__nvim_live_events');"
                "events.onmessage=e=>{if(e.data!==version){events.close();location.reload();}};"
                "addEventListener('pagehide',()=>events.close());})();</script>").encode()
            content, count = re.subn(b'</body\\s*>', lambda match: script+match[0], content, count=1, flags=re.I)
            if not count:
                content += script
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            return io.BytesIO(content)
        if 'If-Modified-Since' in self.headers:
            del self.headers['If-Modified-Since']
        return super().send_head()


if __name__ == '__main__':
    file = Path(sys.argv[1]).resolve(strict=True)
    if not file.is_file():
        raise SystemExit('Abra um arquivo HTML salvo.')
    server = Preview(file)
    url = f'http://127.0.0.1:{server.server_port}/{urllib.parse.quote(file.name)}'
    print('Live: ' + url + ' — Ctrl+C para parar', flush=True)
    try:
        subprocess.Popen(['xdg-open', url], start_new_session=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError:
        print('Abra o endereço acima no navegador.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
