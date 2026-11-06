#!/usr/bin/env python3
"""Exercise titlebar drag, close and border resize on real Wayland terminals."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import importlib.util
spec = importlib.util.spec_from_file_location("terminal_smoke", Path(__file__).with_name("smoke-terminal.py"))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
wait_for, stop = helpers.wait_for, helpers.stop

server_binary, pointer, foot, wtype = [str(Path(arg).resolve()) for arg in sys.argv[1:5]]
processes = []
with tempfile.TemporaryDirectory(prefix="jlinux-frame-") as directory:
    runtime = Path(directory)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, WLR_BACKENDS="headless",
               WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1", JLINUX_TEST_INPUT="1", JLINUX_TILING="0")
    logfile = runtime / "compositor.log"
    def log():
        return logfile.read_text(errors="replace")
    def mouse(*coords):
        subprocess.run([pointer, *map(str, coords)], env=env, check=True, timeout=4)
    try:
        with logfile.open("w") as out:
            server = subprocess.Popen([server_binary], env=env, stdout=out, stderr=subprocess.STDOUT)
            processes.append(server)
            wait_for(lambda: "JLinux compositor pronto:" in log(), [server], "socket")
            env["WAYLAND_DISPLAY"] = next(p.name for p in runtime.glob("wayland-*") if p.is_socket())
            for number in (1, 2, 3):
                title = f"Frame-{number}"
                client = subprocess.Popen([foot, "--config=/dev/null", "--term=xterm-256color", f"--title={title}",
                                           *(["--maximized"] if number == 3 else []), "sh"],
                                          env=env, stdout=out, stderr=subprocess.STDOUT)
                processes.append(client)
                pattern = rf"Janela mapeada: {title} \((\d+)x(\d+) em (\d+),(\d+)\)"
                wait_for(lambda: re.search(pattern, log()), [server, client], "moldura")
                width, height, x, y = map(int, re.search(pattern, log()).groups())
                if number == 1:
                    mouse(x + 160, y - 18, x + 220, y + 2)
                    # Close follows the titlebar's new position after a 60,20 drag.
                    mouse(x + 60 + width - 18, y + 20 - 18)
                    client.wait(timeout=4)
                    # Foot reports 1 when closing its interactive shell sends SIGHUP.
                    if client.returncode not in (0, 1):
                        raise RuntimeError(f"Close retornou {client.returncode}")
                else:
                    def terminal_size(name):
                        marker = runtime / name
                        subprocess.run([wtype, "-s", "200", f"stty size > {marker}", "-k", "Return"], env=env, check=True, timeout=5)
                        wait_for(lambda: marker.exists() and marker.stat().st_size, [server, client], "tamanho do terminal")
                        return tuple(map(int, marker.read_text().split()))
                    if number == 3:
                        if (x, y, width, height) != (3, 36, 1274, 681):
                            raise RuntimeError(f"Maximização inicial não respeitou moldura: {(x, y, width, height)}")
                        subprocess.run([wtype, "-M", "logo", "-k", "Down", "-m", "logo"], env=env, check=True, timeout=4)
                        stop(client)
                        continue
                    before = terminal_size("before")
                    subprocess.run([wtype, "-M", "logo", "-k", "Up", "-m", "logo"], env=env, check=True, timeout=4)
                    maximized = terminal_size("maximized")
                    if not (maximized[0] > before[0] and maximized[1] > before[1]):
                        raise RuntimeError(f"Maximizar falhou: {before} -> {maximized}")
                    mouse(1220, 18)
                    restored = terminal_size("restored")
                    if restored != before:
                        raise RuntimeError(f"Restaurar pelo botão falhou: {before} -> {restored}")
                    # Maximize by its button, then restore with the keyboard.
                    mouse(x + width - 54, y - 18)
                    if terminal_size("button-maximized") != maximized:
                        raise RuntimeError("Botão maximizar não reproduziu a geometria")
                    subprocess.run([wtype, "-M", "logo", "-k", "Down", "-m", "logo"], env=env, check=True, timeout=4)
                    if terminal_size("keyboard-restored") != before:
                        raise RuntimeError("Atalho restaurar não recuperou tamanho")
                    mouse(x + width + 2, y + height + 2, x + width + 66, y + height - 78)
                    time.sleep(.2)
                    after = terminal_size("after")
                    if not (after[0] < before[0] and after[1] > before[1]):
                        raise RuntimeError(f"Borda não redimensionou: {before} -> {after}")
                    stop(client)
            stop(server)
            if server.returncode != 0:
                raise RuntimeError(f"Compositor retornou {server.returncode}")
        print("Molduras: arraste, fechar, maximizar/restaurar, resize e limpeza passaram.")
    except Exception:
        print(log(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes):
            stop(process)

