#!/usr/bin/env python3
"""Exercise real Wayland shell mapping and restart in a private headless session."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time


def wait_for(predicate, processes, description):
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        if any(p.poll() is not None for p in processes):
            raise RuntimeError(f"Processo encerrou antes de {description}")
        if predicate():
            return
        time.sleep(0.05)
    raise RuntimeError(f"Tempo excedido: {description}")


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    compositor, shell = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
    processes = []
    with tempfile.TemporaryDirectory(prefix="jlinux-shell-") as directory:
        runtime = Path(directory)
        os.chmod(runtime, 0o700)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS="headless",
                   WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1",
                   GDK_BACKEND="wayland", GTK_USE_PORTAL="0", NO_AT_BRIDGE="1")
        server_log = runtime / "compositor.log"
        shell_log = runtime / "shell.log"
        def log():
            return server_log.read_text(errors="replace")
        try:
            with server_log.open("w") as server_out, shell_log.open("w") as shell_out:
                server = subprocess.Popen([compositor], env=env, stdout=server_out, stderr=subprocess.STDOUT)
                processes.append(server)
                wait_for(lambda: "JLinux compositor pronto:" in log(), [server], "socket Wayland")
                sockets = list(runtime.glob("wayland-*"))
                socket = next(p for p in sockets if p.is_socket())
                env["WAYLAND_DISPLAY"] = socket.name
                # A disconnected shell must not kill the compositor; restart it.
                for iteration in range(2):
                    client = subprocess.Popen([shell], env=env, stdout=shell_out, stderr=subprocess.STDOUT)
                    processes.append(client)
                    wait_for(lambda: log().count("Interface mapeada: jlinux-panel") > iteration and
                             log().count("Interface mapeada: jlinux-wallpaper") > iteration,
                             [server, client], "mapeamento do painel e wallpaper")
                    time.sleep(0.15)
                    assert client.poll() is None, "Shell encerrou após mapear"
                    stop(client)
                    assert server.poll() is None, "Compositor encerrou com o shell"
                invalid_theme = runtime / "invalid.css"
                invalid_theme.write_text("* { color: not-a-color; }")
                invalid_env = dict(env, JLINUX_THEME=str(invalid_theme))
                invalid = subprocess.Popen([shell], env=invalid_env, stdout=shell_out, stderr=subprocess.STDOUT)
                processes.append(invalid)
                assert invalid.wait(timeout=3) == 1, "Tema inválido deve ser recusado"
                assert server.poll() is None, "Tema inválido derrubou compositor"
                stop(server)
                assert server.returncode == 0, f"Status do compositor: {server.returncode}"
                assert not socket.exists(), "Socket não foi removido"
            print("Shell mapeou wallpaper/painel duas vezes; compositor sobreviveu ao reinício e à rejeição de tema inválido.")
        except Exception:
            for path in (server_log, shell_log):
                if path.exists():
                    print(f"{path.name}:\n{path.read_text(errors='replace')}", file=sys.stderr)
            raise
        finally:
            for process in reversed(processes):
                stop(process)


if __name__ == "__main__":
    main()
