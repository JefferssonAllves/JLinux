#!/usr/bin/env python3
"""Manual hardware check of a running session; switches VT and always returns."""
import argparse
from pathlib import Path
import os
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--session-pid', type=int, required=True)
parser.add_argument('--target-vt', type=int, default=2, choices=range(1, 13))
args = parser.parse_args()
proc = Path(f'/proc/{args.session_pid}')
if Path(os.readlink(proc / 'exe')).name != 'jlinux-session':
    raise SystemExit('PID não pertence a jlinux-session')
original = int(Path('/sys/class/tty/tty0/active').read_text().strip().removeprefix('tty'))
if original == args.target_vt:
    raise SystemExit('Escolha um TTY de teste diferente do atual')
children = [int(pid) for pid in (proc / 'task' / str(args.session_pid) / 'children').read_text().split()]
watched = {args.session_pid: os.readlink(proc / 'exe')}
for pid in children:
    child = Path(f'/proc/{pid}/exe')
    if child.exists():
        watched[pid] = os.readlink(child)
if not any(Path(path).name == 'jlinux-compositor' for path in watched.values()):
    raise SystemExit('Compositor filho não encontrado')
try:
    subprocess.run(['sudo', '-n', 'chvt', str(args.target_vt)], check=True)
    time.sleep(.7)
    for pid, expected in watched.items():
        if os.readlink(f'/proc/{pid}/exe') != expected:
            raise RuntimeError('Processo da sessão mudou durante a troca de VT')
finally:
    subprocess.run(['sudo', '-n', 'chvt', str(original)], check=True)
time.sleep(1)
for pid, expected in watched.items():
    if os.readlink(f'/proc/{pid}/exe') != expected:
        raise RuntimeError('Processo da sessão não foi preservado ao retornar')
print(f'VT{original} -> VT{args.target_vt} -> VT{original}: {len(watched)} processos preservados.')
