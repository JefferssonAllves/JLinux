#!/usr/bin/env python3
"""Check native launcher cleanup after timeout and external termination."""
from pathlib import Path
import select
import shutil
import time
import signal
import subprocess
import sys

binary = str(Path(sys.argv[1]).resolve())
completed = subprocess.run([binary, "--headless", "--no-terminal", "--duration", "0.6"],
                           capture_output=True, text=True, timeout=10)
assert completed.returncode == 0, completed.stdout + completed.stderr
runtime = next(line[6:] for line in completed.stdout.splitlines() if line.startswith("Logs: "))
assert not Path(runtime).exists(), "Runtime não foi removido após duração limitada"
arguments = [binary, "--headless"] + ([] if shutil.which("foot") else ["--no-terminal"])
process = subprocess.Popen(arguments, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
try:
    assert select.select([process.stdout], [], [], 10)[0], "Iniciador não ficou pronto"
    assert process.stdout.readline().startswith("JLinux nativo iniciado."), "Falha ao iniciar"
    runtime = process.stdout.readline().strip().removeprefix("Logs: ")
    if shutil.which("foot"):
        log = Path(runtime) / "compositor.log"
        deadline = time.monotonic() + 6
        while "Janela mapeada: JLinux Terminal" not in log.read_text(errors="replace"):
            assert process.poll() is None and time.monotonic() < deadline, "Iniciador não abriu terminal"
            time.sleep(0.05)
    process.send_signal(signal.SIGTERM)
    output, error = process.communicate(timeout=6)
    assert process.returncode == 0, output + error
    assert not Path(runtime).exists(), "Runtime não foi removido após SIGTERM"
finally:
    if process.poll() is None:
        process.kill()
        process.communicate()
invalid = subprocess.run([binary, "--duration", "nan"], capture_output=True, timeout=3)
assert invalid.returncode == 1, "Duração não finita foi aceita"
print("Iniciador C: duração limitada, SIGTERM e validação passaram; runtime limpo.")
