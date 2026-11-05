#!/usr/bin/env python3
"""Run only this prototype's processes in a private runtime directory."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build-desktop"))
    parser.add_argument("--headless", action="store_true", help="Saída virtual sem janela visível")
    parser.add_argument("--duration", type=float, help="Encerrar após este número de segundos")
    parser.add_argument("--theme", type=Path, help="CSS adicional para personalizar o shell")
    args = parser.parse_args()
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration deve ser positivo")
    build = args.build_dir.resolve()
    for binary in ("jlinux-compositor", "jlinux-shell"):
        if not (build / binary).is_file():
            parser.error(f"Compile primeiro: {build / binary}")
    env = os.environ.copy()
    if args.headless:
        env.update(WLR_BACKENDS="headless", WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1")
    else:
        display = env.get("WAYLAND_DISPLAY")
        runtime = env.get("XDG_RUNTIME_DIR")
        if not display or not runtime:
            parser.error("Execute em uma sessão Wayland ou use --headless")
        # Preserve the parent socket when switching to our private runtime.
        env["WAYLAND_DISPLAY"] = str(Path(runtime) / display)
        env["WLR_BACKENDS"] = "wayland"
    if args.theme:
        if not args.theme.is_file():
            parser.error("Arquivo de tema não encontrado")
        env["JLINUX_THEME"] = str(args.theme.resolve())
    processes = []
    with tempfile.TemporaryDirectory(prefix="jlinux-desktop-") as directory:
        os.chmod(directory, 0o700)
        env["XDG_RUNTIME_DIR"] = directory
        server_log = Path(directory) / "compositor.log"
        shell_log = Path(directory) / "shell.log"
        failed = False
        try:
            with server_log.open("w") as server_out, shell_log.open("w") as shell_out:
                server = subprocess.Popen([str(build / "jlinux-compositor")], env=env,
                                          stdout=server_out, stderr=subprocess.STDOUT)
                processes.append(server)
                deadline = time.monotonic() + 8
                while "JLinux compositor pronto:" not in server_log.read_text(errors="replace"):
                    if server.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("Compositor não inicializou")
                    time.sleep(0.05)
                socket = next(p for p in Path(directory).glob("wayland-*") if p.is_socket())
                env.update(WAYLAND_DISPLAY=socket.name, GDK_BACKEND="wayland", GTK_USE_PORTAL="0")
                client = subprocess.Popen([str(build / "jlinux-shell")], env=env,
                                          stdout=shell_out, stderr=subprocess.STDOUT)
                processes.append(client)
                print("JLinux iniciado. Ctrl+C neste terminal encerra o protótipo.", flush=True)
                end = None if args.duration is None else time.monotonic() + args.duration
                while end is None or time.monotonic() < end:
                    if server.poll() is not None or client.poll() is not None:
                        raise RuntimeError("Um processo do desktop encerrou inesperadamente")
                    time.sleep(0.1)
        except KeyboardInterrupt:
            pass
        except Exception as error:
            failed = True
            print(f"Falha: {error}")
        finally:
            for process in reversed(processes):
                stop(process)
            if failed:
                for path in (server_log, shell_log):
                    print(f"{path.name}:\n{path.read_text(errors='replace')}")
        return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
