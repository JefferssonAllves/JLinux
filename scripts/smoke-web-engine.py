#!/usr/bin/env python3
"""Local HTTP/JS probes; sampled process-tree memory is not a GPU benchmark."""
import argparse
import hashlib
import http.server
import json
import os
import platform
from pathlib import Path
import signal
import ssl
import subprocess
import sys
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    scripts = 0

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/blocked":
            self.send_response(302)
            self.send_header("Location", "file:///etc/passwd")
            self.end_headers()
            return
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()
            return
        if self.path == "/script-ran":
            Handler.scripts += 1
        body = (b"<!doctype html><meta charset='utf-8'><title>JLinux fixture</title>"
                b"<body><input id='entry'><a href='/redirect'>Next</a>"
                b"<script>document.title='JavaScript OK';"
                b"fetch('/script-ran');</script></body>")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def tree_pss(pid):
    pending, seen, total = [pid], set(), 0
    while pending:
        child = pending.pop()
        if child in seen:
            continue
        seen.add(child)
        try:
            pending.extend(map(int, Path(f"/proc/{child}/task/{child}/children").read_text().split()))
            for line in Path(f"/proc/{child}/smaps_rollup").read_text().splitlines():
                if line.startswith("Pss:"):
                    total += int(line.split()[1])
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            pass
    return total


def probe(binary, uri, env):
    with tempfile.TemporaryFile(mode="w+") as output:
        process = subprocess.Popen([binary, "--probe", uri], env=env, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        peak, samples = 0, 0
        deadline = time.monotonic() + 30
        try:
            while process.poll() is None:
                if time.monotonic() > deadline:
                    raise TimeoutError("Web engine probe exceeded 30 seconds")
                peak = max(peak, tree_pss(process.pid))
                samples += 1
                time.sleep(0.025)
            output.seek(0)
            log = output.read()
            assert process.returncode == 0, log
            result = next(json.loads(line) for line in log.splitlines() if line.startswith('{"engine"'))
            assert result["javascript"] is True, result
            document = result["document"]
            assert document["bodyChildren"] >= 2, document
            return dict(result, sampled_peak_tree_pss_kib=peak, samples=samples)
        finally:
            # Include sandbox/network descendants, also when a probe times out.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())
    for bad in ("javascript:alert(1)", "file:///etc/passwd", "https://user:secret@example.org"):
        rejected = subprocess.run([binary, bad], capture_output=True, timeout=5)
        assert rejected.returncode == 2, rejected.stderr
    sandbox = subprocess.run(["bwrap", "--unshare-user", "--ro-bind", "/", "/", "true"],
                             capture_output=True, timeout=5)
    if sandbox.returncode:
        print("SKIP: user namespaces indisponíveis; sandbox NÃO foi desativada.")
        print(sandbox.stderr.decode(errors="replace"))
        return 77
    with tempfile.TemporaryDirectory(prefix="jlinux-web-probe-") as directory:
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CACHE_HOME=directory + "/cache",
                   XDG_DATA_HOME=directory + "/data", XDG_CONFIG_HOME=directory + "/config",
                   LIBGL_ALWAYS_SOFTWARE="1")
        env.pop("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS", None)
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = f"http://127.0.0.1:{server.server_port}"
            results = [probe(binary, base + route, env) for route in ("/", "/redirect", "/")]
            assert Handler.scripts >= 3, "O JavaScript da página não executou em todos os probes"
            blocked = subprocess.run([binary, "--probe", base + "/blocked"], env=env,
                                     capture_output=True, timeout=25)
            assert blocked.returncode == 1, blocked.stderr
            cert, key = Path(directory) / "cert.pem", Path(directory) / "key.pem"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=localhost", "-keyout", str(key), "-out", str(cert)],
                           check=True, capture_output=True, timeout=10)
            tls = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(cert, key)
            tls.socket = context.wrap_socket(tls.socket, server_side=True)
            tls_thread = threading.Thread(target=tls.serve_forever, daemon=True)
            tls_thread.start()
            try:
                invalid_tls = subprocess.run([binary, "--probe", f"https://127.0.0.1:{tls.server_port}/"],
                                             env=env, capture_output=True, timeout=25)
                assert invalid_tls.returncode == 1, invalid_tls.stderr
            finally:
                tls.shutdown(); tls.server_close(); tls_thread.join()
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
    with open(binary, "rb") as executable:
        digest = hashlib.file_digest(executable, "sha256").hexdigest()
    report = dict(binary_sha256=digest, kernel=platform.release(),
                  backend="WPE headless / software solicitado", workload="HTTP local + DOM + JS + redirect",
                  security_checks="esquemas locais/ativos e redirecionamento file bloqueados; certificado autoassinado recusado",
                  note="3 processos novos; PSS amostrado a cada 25 ms; sem medição GPU/energia ou comparação de motores",
                  runs=results)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if args.output:
        args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
