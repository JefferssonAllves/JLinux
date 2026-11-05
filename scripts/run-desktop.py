#!/usr/bin/env python3
"""Run only this prototype's processes in a private runtime directory."""
import argparse
import os
from pathlib import Path
import subprocess
import shutil
import signal
import select
import tempfile
import time


def stop(process):
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build-interactive"))
    parser.add_argument("--headless", action="store_true", help="Saída virtual sem janela visível")
    parser.add_argument("--drm", action="store_true", help="Executar no monitor a partir do TTY")
    parser.add_argument("--codex", action="store_true", help="Abrir Codex dentro do primeiro terminal")
    parser.add_argument("--no-terminal", action="store_true", help="Não abrir terminal inicial")
    parser.add_argument("--keyboard-layout", default="br", help="Layout XKB (padrão: br; use us para americano)")
    parser.add_argument("--duration", type=float, help="Encerrar após este número de segundos")
    parser.add_argument("--theme", type=Path, help="CSS adicional para personalizar o shell")
    args = parser.parse_args()
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration deve ser positivo")
    build = args.build_dir.resolve()
    for binary in ("jlinux-compositor", "jlinux-shell"):
        if not (build / binary).is_file():
            parser.error(f"Compile primeiro: {build / binary}")
    if args.headless and args.drm:
        parser.error("Escolha --headless ou --drm")
    env = os.environ.copy()
    env["PATH"] = str(Path.home() / ".local/bin") + os.pathsep + env.get("PATH", "")
    env["XKB_DEFAULT_LAYOUT"] = args.keyboard_layout
    if not args.no_terminal and not shutil.which("foot", path=env["PATH"]):
        parser.error("Instale foot para abrir o terminal")
    if args.codex and not shutil.which("codex", path=env["PATH"]):
        parser.error("Codex CLI não encontrado no PATH")
    if args.headless:
        env.update(WLR_BACKENDS="headless", WLR_RENDERER="pixman", WLR_HEADLESS_OUTPUTS="1")
    elif args.drm:
        if env.get("WAYLAND_DISPLAY") or env.get("DISPLAY"):
            parser.error("Use --drm a partir do TTY, fora de outra sessão gráfica")
        env["WLR_BACKENDS"] = "drm,libinput"
        env.pop("WLR_RENDERER", None)
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
                                          stdout=server_out, stderr=subprocess.STDOUT, start_new_session=True)
                processes.append(server)
                deadline = time.monotonic() + 8
                while "JLinux compositor pronto:" not in server_log.read_text(errors="replace"):
                    if server.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("Compositor não inicializou")
                    time.sleep(0.05)
                socket = next(p for p in Path(directory).glob("wayland-*") if p.is_socket())
                env.update(WAYLAND_DISPLAY=socket.name, GDK_BACKEND="wayland", GTK_USE_PORTAL="0")
                client = subprocess.Popen([str(build / "jlinux-shell")], env=env,
                                          stdout=shell_out, stderr=subprocess.STDOUT, start_new_session=True)
                processes.append(client)
                if not args.no_terminal:
                    terminal_args = ["foot", "--term=xterm-256color", "--title=JLinux Terminal",
                                     "--working-directory=" + str(Path(__file__).resolve().parents[1])]
                    if args.codex:
                        terminal_args += ["codex"]
                    terminal = subprocess.Popen(terminal_args, env=env, stdout=shell_out,
                                                stderr=subprocess.STDOUT, start_new_session=True)
                    processes.append(terminal)
                    terminal_deadline = time.monotonic() + 8
                    while "Janela mapeada:" not in server_log.read_text(errors="replace"):
                        if terminal.poll() is not None or server.poll() is not None or time.monotonic() > terminal_deadline:
                            raise RuntimeError("Terminal não abriu")
                        time.sleep(0.05)
                print("JLinux iniciado. Super+Enter: terminal; Super+Esc: sair; Alt+Tab: alternar janelas.", flush=True)
                descriptors = []
                try:
                    descriptors = [os.pidfd_open(server.pid), os.pidfd_open(client.pid)]
                    ready, _, _ = select.select(descriptors, [], [], args.duration)
                    if ready and server.poll() != 0:
                        raise RuntimeError("Um processo do desktop encerrou inesperadamente")
                finally:
                    for descriptor in descriptors:
                        os.close(descriptor)
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
