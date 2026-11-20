#!/usr/bin/env python3
"""Exercise microphone buttons with private audio fixtures; never open real audio devices."""
import json
import math
import os
from pathlib import Path
import runpy
import signal
import socket
import subprocess
import sys
import tempfile
import time
h = runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait, stop = h['wait_for'], h['stop']
compositor, shell, pointer = [str(Path(a).resolve()) for a in sys.argv[1:4]]
processes = []
with tempfile.TemporaryDirectory(prefix='jlinux-microphone-test-') as directory:
    root = Path(directory)
    commands = root / 'commands'
    commands.mkdir()
    calls, mode, logfile = root / 'calls', root / 'mode', root / 'session.log'
    mode.write_text('hold')
    mock = commands / 'pw-record'
    mock.write_text(f'''#!{sys.executable}
import json,os,pathlib,signal,sys,time,wave
root=pathlib.Path(os.environ['MIC_TEST'])
assert not signal.pthread_sigmask(signal.SIG_BLOCK, [])
assert os.environ.get('PIPEWIRE_RUNTIME_DIR')
with (root/'calls').open('a') as out: out.write(json.dumps([sys.argv,os.getpid()])+'\\n')
mode=(root/'mode').read_text()
while mode=='hold':
 time.sleep(.02);mode=(root/'mode').read_text()
if mode=='fail':sys.exit(2)
if pathlib.Path(sys.argv[0]).name=='pw-record':
 assert sys.argv[1:-1]==['--rate','48000','--channels','1','--format','s16','--sample-count','384000']
 with wave.open(sys.argv[-1],'wb') as out:
  out.setnchannels(1);out.setsampwidth(2);out.setframerate(48000)
  out.writeframes(b'\\0\\0'*(100 if mode=='short' else 384000))
 sys.exit(1)
with wave.open(sys.argv[-1],'rb') as audio:assert audio.getnframes()==384000
''')
    mock.chmod(0o755)
    (commands / 'pw-play').symlink_to(mock)
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=str(root / 'config'),
               XDG_DATA_HOME=str(root / 'data'), TMPDIR=directory,
               WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
               JLINUX_TEST_INPUT='1', MIC_TEST=directory,
               PATH=str(commands) + os.pathsep + os.environ['PATH'])
    def log(): return logfile.read_text(errors='replace')
    def recorded(): return [json.loads(x) for x in calls.read_text().splitlines()] if calls.exists() else []
    scale = min(1280 / 1672, (720 - 52) / 889)
    dx, dy = (1280 - 1672 * scale) / 2, 52 + (720 - 52 - 889 * scale) / 2 - 52 * scale
    def click(x, y):
        subprocess.run([pointer, str(round(dx + x * scale)), str(round(dy + y * scale))], env=env, check=True, timeout=4)
    def open_settings():
        before = log().count('Dock circular aberto.')
        with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as ctl:
            ctl.sendto(b'show', str(root / 'jlinux-shell-control'))
        wait(lambda: log().count('Dock circular aberto.') > before, processes, 'dock')
        time.sleep(.25)
        angle = math.radians(-90 + 4 * 360 / 10)
        click(836 + 250 * math.cos(angle), 469 + 250 * math.sin(angle))
        click(450, 575)
    def action(x, expected):
        before = log().count(expected)
        click(x, 440)
        wait(lambda: log().count(expected) > before, processes, expected)
    def finished(expected, before):
        wait(lambda: log().count(expected) > before, processes, expected)
    def gone(pid):
        try: os.kill(pid, 0); return False
        except ProcessLookupError: return True
    try:
        with logfile.open('w') as out:
            server = subprocess.Popen([compositor], env=env, stdout=out, stderr=out); processes.append(server)
            wait(lambda: 'JLinux compositor pronto:' in log(), processes, 'compositor')
            env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
            client = subprocess.Popen([shell], env=env, stdout=out, stderr=out); processes.append(client)
            wait(lambda: 'Interface mapeada: jlinux-panel' in log(), processes, 'shell')
            open_settings()
            click(1100, 440); time.sleep(.1)
            assert not recorded(), 'playback before recording'
            before = log().count('gravação pronta')
            action(800, 'Teste de microfone: gravando.')
            wait(lambda: len(recorded()) == 1, processes, 'record process')
            path = Path(recorded()[0][0][-1])
            assert path.stat().st_mode & 0o777 == 0o600
            assert path.parent.stat().st_mode & 0o777 == 0o700
            click(800, 440); click(1100, 440); time.sleep(.1)
            assert len(recorded()) == 1, 'overlapping record/playback'
            mode.write_text('normal'); finished('gravação pronta', before)
            for _ in range(2):
                before = log().count('reprodução concluída')
                action(1100, 'Teste de microfone: reproduzindo.')
                finished('reprodução concluída', before)
            for failure in ('short', 'fail'):
                mode.write_text(failure); before = log().count('Teste de microfone: falhou.')
                action(800, 'Teste de microfone: gravando.'); finished('Teste de microfone: falhou.', before)
                count = len(recorded()); click(1100, 440); time.sleep(.1)
                assert len(recorded()) == count, 'failed capture became playable'
            mode.write_text('normal'); before = log().count('gravação pronta')
            action(800, 'Teste de microfone: gravando.'); finished('gravação pronta', before)
            mode.write_text('fail'); before = log().count('Teste de microfone: falhou.')
            action(1100, 'Teste de microfone: reproduzindo.'); finished('Teste de microfone: falhou.', before)
            mode.write_text('normal'); before = log().count('reprodução concluída')
            action(1100, 'Teste de microfone: reproduzindo.'); finished('reprodução concluída', before)
            mode.write_text('hold'); before = log().count('Teste de microfone: falhou.')
            action(800, 'Teste de microfone: gravando.')
            deadline = time.monotonic() + 14
            while log().count('Teste de microfone: falhou.') == before and time.monotonic() < deadline:
                assert client.poll() is None
                time.sleep(.05)
            assert log().count('Teste de microfone: falhou.') > before, 'stalled capture was not stopped'
            mode.write_text('hold'); count = len(recorded())
            action(800, 'Teste de microfone: gravando.')
            wait(lambda: len(recorded()) > count, processes, 'recording before tab change')
            last = recorded()[-1]; path = Path(last[0][-1]); click(450, 480)
            wait(lambda: gone(last[1]) and not path.parent.exists(), processes, 'tab change cancels recording')
            click(450, 575)
            mode.write_text('hold'); count = len(recorded())
            action(800, 'Teste de microfone: gravando.')
            wait(lambda: len(recorded()) > count, processes, 'pending recording')
            last = recorded()[-1]; path = Path(last[0][-1]); click(1335, 180)
            wait(lambda: gone(last[1]) and not path.parent.exists(), processes, 'close cancels and deletes recording')
            open_settings(); mode.write_text('hold'); count = len(recorded())
            action(800, 'Teste de microfone: gravando.')
            wait(lambda: len(recorded()) > count, processes, 'shutdown recording')
            last = recorded()[-1]; path = Path(last[0][-1])
            stop(client); processes.remove(client)
            assert gone(last[1]) and not path.parent.exists(), 'shell shutdown leaked capture or audio'
    except Exception:
        print(log(), recorded(), file=sys.stderr)
        raise
    finally:
        for process in reversed(processes): stop(process)
print('Microphone: record/play, disabled states, private files, failures, retries and cleanup passed.')
