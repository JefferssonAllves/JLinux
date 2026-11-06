#!/usr/bin/env python3
"""Exercise native address input and WPE page input on isolated JLinux Wayland."""
import http.server
import os
from pathlib import Path
import runpy
import re
import subprocess
import sys
import tempfile
import threading
helpers = runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop, wait_for = helpers['stop'], helpers['wait_for']


class Handler(http.server.BaseHTTPRequestHandler):
    events = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        Handler.events.append(self.path)
        body = b"""<!doctype html><meta charset='utf-8'><title>Browser fixture</title>
        <body><button style='position:fixed;left:8px;top:0' onclick="fetch('/page-click')">tap</button><button style='position:fixed;left:8px;bottom:0' onclick="fetch('/bottom-click')">bottom</button><input autofocus oninput="fetch('/typed?value='+encodeURIComponent(this.value))">
        <script src='/pagead/banner.js'></script><script>addEventListener('pageshow',()=>fetch('/shown?path='+location.pathname));
        function viewport(){fetch('/viewport?w='+innerWidth+'&h='+innerHeight);}
        addEventListener('resize',viewport); viewport();</script></body>"""
        mime = "text/html; charset=utf-8"
        if self.path == "/pagead/banner.js":
            body, mime = b"window.adLoaded = true;", "application/javascript"
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    compositor, browser, wtype, pointer = [str(Path(arg).resolve()) for arg in sys.argv[1:5]]
    sandbox = subprocess.run(["bwrap", "--unshare-user", "--ro-bind", "/", "/", "true"], capture_output=True)
    if sandbox.returncode:
        print("SKIP: namespaces indisponíveis; sandbox preservada")
        return 77
    processes = []
    with tempfile.TemporaryDirectory(prefix="jlinux-web-wayland-") as directory:
        runtime = Path(directory)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CACHE_HOME=directory + "/cache",
                   XDG_DATA_HOME=directory + "/data", XDG_CONFIG_HOME=directory + "/config",
                   WLR_BACKENDS="headless", WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1",
                   JLINUX_TEST_INPUT="1", LIBGL_ALWAYS_SOFTWARE="1", JLINUX_BROWSER_ADBLOCK="1")
        logfile = runtime / "session.log"
        web_server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=web_server.serve_forever, daemon=True)
        thread.start()
        def log():
            return logfile.read_text(errors="replace")
        def keys(*args):
            subprocess.run([wtype, "-s", "100", *args], env=env, check=True, timeout=5)
        def navigate(path):
            before = Handler.events.count("/shown?path=" + path)
            keys("-M", "ctrl", "-k", "l", "-m", "ctrl", "-d", "2", base + path, "-k", "Return")
            wait_for(lambda: Handler.events.count("/shown?path=" + path) > before, processes, "página " + path)
        def typed(text, expected):
            keys("-k", "Escape", "-d", "10", text)
            wait_for(lambda: "/typed?value=" + expected in Handler.events, processes, "estado da aba " + expected)
        def click(x, y, *end):
            subprocess.run([pointer, str(x), str(y), *map(str, end)], env=env, check=True, timeout=5)
        try:
            with logfile.open("w") as output:
                server = subprocess.Popen([compositor], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(server)
                wait_for(lambda: "JLinux compositor pronto:" in log(), processes, "socket Wayland")
                env["WAYLAND_DISPLAY"] = next(p.name for p in runtime.glob("wayland-*") if p.is_socket())
                base = f"http://127.0.0.1:{web_server.server_port}"
                client = subprocess.Popen([browser, base + "/first"], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(client)
                wait_for(lambda: "Janela mapeada:" in log() and "/shown?path=/first" in Handler.events,
                         processes, "janela e primeira página")
                dimensions = re.search(r"Janela mapeada:.*\((\d+)x(\d+)", log())
                window_width, window_height = map(int, dimensions.groups())
                wait_for(lambda: f"/viewport?w={window_width}&h={window_height - 100}" in Handler.events,
                         processes, "viewport desconta a altura do cabeçalho")
                assert "/pagead/banner.js" not in Handler.events, "Default blocking did not prevent download"
                before = Handler.events.count("/shown?path=/first")
                keys("-M", "ctrl", "-M", "shift", "-k", "b", "-m", "shift", "-m", "ctrl")
                wait_for(lambda: Handler.events.count("/shown?path=/first") > before,
                         processes, "desativar bloqueio e recarregar")
                assert Handler.events.count("/pagead/banner.js") == 1, Handler.events
                keys("-M", "ctrl", "-k", "t", "-m", "ctrl")
                navigate("/ad-new-tab")
                assert Handler.events.count("/pagead/banner.js") == 1, "Exceção vazou para outra aba"
                keys("-M", "ctrl", "-k", "w", "-m", "ctrl")
                before = Handler.events.count("/shown?path=/first")
                keys("-M", "ctrl", "-M", "shift", "-k", "b", "-m", "shift", "-m", "ctrl")
                wait_for(lambda: Handler.events.count("/shown?path=/first") > before,
                         processes, "reativar bloqueio pelo atalho")
                assert Handler.events.count("/pagead/banner.js") == 1, "Ad was downloaded after re-enabling"
                keys("-k", "Escape", "-d", "10", "native")
                wait_for(lambda: "/typed?value=native" in Handler.events, processes, "entrada na página")
                click(75, 190)
                wait_for(lambda: "/page-click" in Handler.events, processes, "primeiro clique na página")
                # The address field remains clickable after navigation and Escape, without Ctrl+L.
                click(340, 165)
                keys("-d", "2", base + "/second", "-k", "Return")
                wait_for(lambda: "/shown?path=/second" in Handler.events, processes, "endereço nativo")
                before = Handler.events.count("/second")
                keys("-M", "ctrl", "-k", "r", "-m", "ctrl")
                wait_for(lambda: Handler.events.count("/second") > before, processes, "recarregar")
                before = Handler.events.count("/shown?path=/first")
                keys("-M", "alt", "-k", "Left", "-m", "alt")
                wait_for(lambda: Handler.events.count("/shown?path=/first") > before, processes, "voltar")
                before = Handler.events.count("/shown?path=/second")
                keys("-M", "alt", "-k", "Right", "-m", "alt")
                wait_for(lambda: Handler.events.count("/shown?path=/second") > before, processes, "avançar")
                typed("keep", "keep")
                first_loads = Handler.events.count("/second")
                keys("-M", "ctrl", "-k", "t", "-m", "ctrl")
                navigate("/tab-two")
                navigate("/tab-two-history")
                before = Handler.events.count("/shown?path=/tab-two")
                keys("-M", "alt", "-k", "Left", "-m", "alt")
                wait_for(lambda: Handler.events.count("/shown?path=/tab-two") > before, processes, "histórico da segunda aba")
                typed("two", "two")
                keys("-M", "ctrl", "-k", "Tab", "-m", "ctrl")
                typed("one", "keepone")
                assert Handler.events.count("/second") == first_loads, "Alternar aba recarregou a página"
                keys("-M", "ctrl", "-M", "shift", "-k", "Tab", "-m", "shift", "-m", "ctrl")
                typed("more", "twomore")
                keys("-M", "ctrl", "-k", "t", "-m", "ctrl")
                navigate("/third")
                keys("-M", "ctrl", "-k", "w", "-m", "ctrl")
                typed("x", "twomorex")
                # The window/header origin is 40,80; the page starts 100 px below it.
                click(110, 110)
                typed("y", "keeponey")
                click(40 + 12 + 2 * 180 + 16, 110)  # + beside the second tab.
                navigate("/mouse-third")
                click(222, 110)  # Close the inactive first tab.
                typed("third", "third")
                click(402, 110)  # Close the active second tab, keeping tab-two.
                typed("x", "twomorexx")
                for _ in range(6):
                    keys("-M", "ctrl", "-k", "t", "-m", "ctrl")
                keys("-M", "ctrl", "-k", "Tab", "-m", "ctrl")
                typed("z", "twomorexxz")
                keys("-M", "ctrl", "-M", "shift", "-k", "Tab", "-m", "shift", "-m", "ctrl")
                for _ in range(6):
                    keys("-M", "ctrl", "-k", "w", "-m", "ctrl")
                assert f"/viewport?w={window_width}&h={window_height}" not in Handler.events, "A página recebeu a altura do cabeçalho ao alternar abas"
                click(75, 80 + window_height - 10)
                wait_for(lambda: "/bottom-click" in Handler.events, processes, "rodapé da página visível")
                keys("-M", "logo", "-k", "Up", "-m", "logo")
                wait_for(lambda: "Janela maximizada:" in log(), processes, "maximização")
                maximized = re.search(r"Janela maximizada:.*\((\d+)x(\d+) em (\d+),(\d+)", log())
                width, height, x, y = map(int, maximized.groups())
                wait_for(lambda: f"/viewport?w={width}&h={height - 100}" in Handler.events,
                         processes, "viewport maximizado desconta o cabeçalho")
                before = Handler.events.count("/page-click")
                click(x + 35, y + 110)
                wait_for(lambda: Handler.events.count("/page-click") > before, processes, "topo após maximizar")
                before = Handler.events.count("/bottom-click")
                click(x + 35, y + height - 10)
                wait_for(lambda: Handler.events.count("/bottom-click") > before, processes, "rodapé após maximizar")
                click(x + 12 + 180 + 16, y + 18)  # + follows the remaining tab after maximizing.
                keys("-M", "ctrl", "-k", "w", "-m", "ctrl")
                before = Handler.events.count(f"/viewport?w={window_width}&h={window_height - 100}")
                keys("-M", "logo", "-k", "Down", "-m", "logo")
                wait_for(lambda: Handler.events.count(f"/viewport?w={window_width}&h={window_height - 100}") > before,
                         processes, "restaurar viewport")
                click(40 + window_width + 2, 80 + window_height + 2,
                      40 + window_width + 66, 80 + window_height - 78)
                wait_for(lambda: f"/viewport?w={window_width + 64}&h={window_height - 80 - 100}" in Handler.events,
                         processes, "viewport após redimensionar pela borda")
                keys("-M", "ctrl", "-k", "l", "-m", "ctrl", "-k", "Escape")
                keys("-M", "ctrl", "-k", "q", "-m", "ctrl")
                assert client.wait(timeout=5) == 0, log()
                assert server.poll() is None, log()
                mapped = log().count("Janela mapeada:")
                client = subprocess.Popen([browser], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(client)
                wait_for(lambda: log().count("Janela mapeada:") > mapped, [server, client], "segunda janela")
                keys("-M", "ctrl", "-k", "w", "-m", "ctrl")
                assert client.wait(timeout=5) == 0, log()
                mapped = log().count("Janela mapeada:")
                client = subprocess.Popen([browser], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(client)
                wait_for(lambda: log().count("Janela mapeada:") > mapped, [server, client], "terceira janela")
                keys("-M", "ctrl", "-k", "t", "-m", "ctrl")
                keys("-M", "logo", "-k", "q", "-m", "logo")
                assert client.wait(timeout=5) == 0, log()
                assert server.poll() is None, log()
                stop(server)
                assert server.returncode == 0, log()
            print("Wayland: navegação, abas por teclado/mouse, histórico e estado independentes, 7 abas, fechar ativa/inativa/última, viewport abaixo do cabeçalho, topo/rodapé clicáveis, maximização/restauração/resize e saída passaram.")
        except Exception:
            print(log(), file=sys.stderr)
            print(Handler.events[-30:], file=sys.stderr)
            raise
        finally:
            for process in reversed(processes):
                stop(process)
            web_server.shutdown(); web_server.server_close(); thread.join()
    return 0


if __name__ == "__main__":
    sys.exit(main())
