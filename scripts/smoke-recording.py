#!/usr/bin/env python3
"""Shift+Print creates a playable video in an isolated compositor, never the user's desktop."""
import json
import os
from pathlib import Path
import runpy
import signal
import socket
import subprocess
import sys
import tempfile
import time
helpers=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait,stop=helpers['wait_for'],helpers['stop']
compositor,shell,wtype,foot=[str(Path(value).resolve()) for value in sys.argv[1:5]]
with tempfile.TemporaryDirectory(prefix='jlinux-recording-') as directory:
    root=Path(directory);home=root/'home';home.mkdir();config=root/'config';config.mkdir()
    (config/'user-dirs.dirs').write_text('XDG_VIDEOS_DIR="'+str(home/'Videos')+'"\n')
    env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(config),XDG_RUNTIME_DIR=directory,
             WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
    processes=[];log=root/'session.log'
    def content():return log.read_text(errors='replace')
    def shortcut():subprocess.run([wtype,'-M','shift','-k','Print','-m','shift'],env=env,check=True,timeout=4)
    try:
        with log.open('w') as output:
            server=subprocess.Popen([compositor],env=env,stdout=output,stderr=output);processes.append(server)
            wait(lambda:'JLinux compositor pronto:' in content(),processes,'compositor')
            env['WAYLAND_DISPLAY']=next(path.name for path in root.glob('wayland-*') if path.is_socket())
            client=subprocess.Popen([shell],env=env,stdout=output,stderr=output);processes.append(client)
            wait(lambda:'Interface mapeada: jlinux-panel' in content(),processes,'panel')
            terminal=subprocess.Popen([foot,'--config=/dev/null','--title=Recording fixture','sh','-c',
                'while :; do printf "\\rRecording fixture %s" "$(date +%s)"; sleep .2; done'],env=env,stdout=output,stderr=output);processes.append(terminal)
            wait(lambda:'Janela mapeada: Recording fixture' in content(),processes,'test window')
            shortcut()
            wait(lambda:bool(list(home.rglob('*.mp4'))),processes,'video file')
            time.sleep(2)
            subprocess.run(['grim','/tmp/jlinux-recording-active.png'],env=env,check=True,timeout=5)
            shortcut()
            wait(lambda:'Gravação salva:' in content(),processes,'video finalized')
            files=list(home.rglob('*.mp4'));assert len(files)==1
            result=subprocess.run(['ffprobe','-v','error','-show_streams','-show_format','-of','json',str(files[0])],capture_output=True,text=True,check=True,timeout=5)
            metadata=json.loads(result.stdout);video=next(stream for stream in metadata['streams'] if stream['codec_type']=='video')
            assert video['codec_name']=='h264' and video['width']==1280 and video['height']==720
            assert float(metadata['format']['duration'])>0 and int(video['nb_frames'])>0
            assert files[0].stat().st_mode & 0o077==0
            subprocess.run(['grim','/tmp/jlinux-recording-stopped.png'],env=env,check=True,timeout=5)
            shortcut();time.sleep(1)
            stop(client)
            assert len(list(home.rglob('*.mp4')))==2
            second=sorted(home.rglob('*.mp4'),key=lambda p:p.stat().st_mtime)[-1]
            subprocess.run(['ffprobe','-v','error',str(second)],check=True,timeout=5)
            processes.remove(client)
            commands=root/'bin';commands.mkdir()
            marker=root/'encoder-failed'
            (commands/'wf-recorder').write_text('#!/bin/sh\n: > "$JLINUX_RECORD_TEST_MARKER"\nexit 1\n')
            (commands/'wf-recorder').chmod(0o755)
            failed_env=dict(env,PATH=str(commands)+os.pathsep+env['PATH'],JLINUX_RECORD_TEST_MARKER=str(marker))
            failed=subprocess.Popen([shell],env=failed_env,stdout=output,stderr=output);processes.append(failed)
            wait(lambda:content().count('Interface mapeada: jlinux-panel')>=2,processes,'restarted shell')
            shortcut()
            wait(lambda:marker.exists(),processes,'failing encoder attempted')
            wait(lambda:len(list(home.rglob('*.mp4')))==2,processes,'empty failed capture removed')
            assert all(path.stat().st_size>0 for path in home.rglob('*.mp4'))

    except Exception:
        print(content(),file=sys.stderr);raise
    finally:
        for process in reversed(processes):stop(process)
print('Recording: Shift+Print, playable private MP4, repeated session and graceful shell exit passed.')
