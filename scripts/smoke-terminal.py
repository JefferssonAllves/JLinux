#!/usr/bin/env python3
"""Verify real terminal input, click-to-focus and window lifecycle in private Wayland."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def wait_for(condition, processes, description):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if any(p.poll() is not None for p in processes):
            raise RuntimeError(f"Processo encerrou: {description}")
        if condition():
            return
        time.sleep(0.05)
    raise RuntimeError(f"Tempo excedido: {description}")


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    compositor, pointer_client, foot, wtype = [str(Path(arg).resolve()) for arg in sys.argv[1:5]]
    processes = []
    with tempfile.TemporaryDirectory(prefix="jlinux-input-") as directory:
        runtime = Path(directory)
        os.chmod(runtime, 0o700)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS="headless",
                   WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1", JLINUX_TEST_INPUT="1")
        logfile = runtime / "session.log"
        def log():
            return logfile.read_text(errors="replace")
        try:
            with logfile.open("w") as output:
                server = subprocess.Popen([compositor], env=env, stdout=output, stderr=subprocess.STDOUT)
                processes.append(server)
                wait_for(lambda: "JLinux compositor pronto:" in log(), [server], "servidor")
                env["WAYLAND_DISPLAY"] = next(p.name for p in runtime.glob("wayland-*") if p.is_socket())
                for index in (1, 2):
                    terminal_env = dict(env, TEST_ID=str(index))
                    client = subprocess.Popen([foot, "--config=/dev/null", "--term=xterm-256color",
                                               f"--title=Terminal-{index}", "sh"], env=terminal_env,
                                              stdout=output, stderr=subprocess.STDOUT)
                    processes.append(client)
                    wait_for(lambda: f"Janela mapeada: Terminal-{index}" in log(), [server, client], "terminal")
                marker = runtime / "typed"
                command = f'printf "$TEST_ID" > {marker}'
                subprocess.run([wtype, "-s", "200", "-d", "2", command, "-k", "Return"], env=env, check=True, timeout=6)
                wait_for(lambda: marker.exists() and marker.read_text() != "", processes, "digitação no terminal focado")
                assert marker.read_text() == "2", "Última janela não recebeu foco"
                marker.unlink()
                # Terminal 1 begins at 40,80; Terminal 2 at 68,108.
                subprocess.run([pointer_client, "50", "90"], env=env, check=True, timeout=3)
                subprocess.run([wtype, "-s", "200", "-d", "2", command, "-k", "Return"], env=env, check=True, timeout=6)
                wait_for(lambda: marker.exists() and marker.read_text() != "", processes, "foco por clique")
                assert marker.read_text() == "1", "Clique não transferiu foco para a primeira janela"
                # Destroy the focused client; remaining client must receive input.
                stop(processes[1])
                marker.unlink()
                subprocess.run([wtype, "-s", "200", "-d", "2", command, "-k", "Return"], env=env, check=True, timeout=6)
                wait_for(lambda: marker.exists() and marker.read_text() != "", [server, processes[2]], "recuperação de foco")
                assert marker.read_text() == "2", "Foco perdido após fechar janela"
                stop(processes[2])
                subprocess.run([wtype, "-s", "200", "-M", "logo", "-k", "Escape", "-m", "logo"],
                               env=env, timeout=6, stdout=output, stderr=subprocess.STDOUT)
                server.wait(timeout=3)
                assert server.returncode == 0, f"Compositor: {server.returncode}"
            print("Terminais reais: digitação, clique, troca de foco e desconexão passaram.")
        except Exception:
            print(log(), file=sys.stderr)
            raise
        finally:
            for process in reversed(processes):
                stop(process)


if __name__ == "__main__":
    main()
