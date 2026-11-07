#!/usr/bin/env python3
"""Measure an unlinked WhatsApp window in isolated headless Wayland (not logged-in usage)."""
import json
import hashlib
import signal
import os
from pathlib import Path
import platform
import runpy
import statistics
import subprocess
import sys
import tempfile
import time
helpers=runpy.run_path(str(Path(__file__).with_name('smoke-terminal.py')))
stop,wait=helpers['stop'],helpers['wait_for']
compositor,browser,output=map(lambda x:str(Path(x).resolve()),sys.argv[1:4])
backend=sys.argv[4] if len(sys.argv)>4 else 'gles2'
assert backend in ('gles2','pixman')

def tree_pss(root):
    children={};ticks={}
    for item in Path('/proc').iterdir():
        if not item.name.isdigit(): continue
        try:
            fields=(item/'stat').read_text().rsplit(')',1)[1].split()
            ticks[int(item.name)]=int(fields[11])+int(fields[12])
            children.setdefault(int(fields[1]),[]).append(int(item.name))
        except (OSError,ValueError): pass
    pending,seen,total,unreadable,swap,cpu=[root],set(),0,0,0,0
    while pending:
        pid=pending.pop()
        if pid in seen: continue
        cpu+=ticks.get(pid,0)
        seen.add(pid);pending.extend(children.get(pid,[]))
        try:
            text=Path(f'/proc/{pid}/smaps_rollup').read_text()
        except PermissionError:
            result=subprocess.run(['sudo','-n','cat',f'/proc/{pid}/smaps_rollup'],capture_output=True,text=True)
            if result.returncode: unreadable+=1;continue
            text=result.stdout
        except (FileNotFoundError,ProcessLookupError): continue
        total+=next(int(line.split()[1]) for line in text.splitlines() if line.startswith('Pss:'))
        swap+=next((int(line.split()[1]) for line in text.splitlines() if line.startswith('SwapPss:')),0)
    return total,unreadable,swap,cpu

result={'workload':'official WhatsApp Web, fresh unlinked profile, no messages/calls',
        'backend':f'headless Wayland 1280x720, {backend} compositor; WPE '+('software GL' if backend=='pixman' else 'default GL driver'),
        'engine':subprocess.check_output(['pkg-config','--modversion','wpe-webkit-2.0'],text=True).strip(),
        'kernel':platform.release(),'binary_sha256':hashlib.sha256(Path(browser).read_bytes()).hexdigest(),'runs':[]}
for iteration in range(2):
    processes=[]
    with tempfile.TemporaryDirectory(prefix='jlinux-whatsapp-memory-') as directory:
        root=Path(directory)
        env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_DATA_HOME=directory+'/data',
            XDG_CACHE_HOME=directory+'/cache',XDG_CONFIG_HOME=directory+'/config',
            WLR_BACKENDS='headless',WLR_RENDERER=backend,WLR_HEADLESS_OUTPUTS='1')
        if backend=='pixman': env['LIBGL_ALWAYS_SOFTWARE']='1'
        else: env.pop('LIBGL_ALWAYS_SOFTWARE',None)
        logfile=root/'app.log'
        def log(): return logfile.read_text(errors='replace')
        try:
            with logfile.open('w') as out:
                server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True);processes.append(server)
                wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
                env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
                app=subprocess.Popen([browser,'--whatsapp'],env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True);processes.append(app)
                start=time.monotonic();samples=[]
                while time.monotonic()-start<25:
                    assert app.poll() is None,log()
                    pss,missing,swap,cpu=tree_pss(app.pid)
                    assert pss>0 and missing==0, 'Incomplete PSS measurement'
                    samples.append({'seconds':round(time.monotonic()-start,2),'pss_kib':pss,'swap_pss_kib':swap,'cpu_ticks':cpu})
                    time.sleep(.5)
                assert 'Página carregada' in log() and 'Falha de carregamento' not in log() and 'Processo web encerrado' not in log(),log()
                steady=[s['pss_kib'] for s in samples if s['seconds']>=17]
                idle=[s for s in samples if s['seconds']>=17]
                cpu_pct=(idle[-1]['cpu_ticks']-idle[0]['cpu_ticks'])/os.sysconf('SC_CLK_TCK')/(idle[-1]['seconds']-idle[0]['seconds'])*100
                result['runs'].append({'sampled_peak_pss_kib':max(s['pss_kib'] for s in samples),
                    'last_8_seconds_median_pss_kib':statistics.median(steady),'last_8_seconds_cpu_percent_one_core':round(cpu_pct,2),'samples':samples})
                print(json.dumps({k:v for k,v in result['runs'][-1].items() if k!='samples'}),flush=True)
        finally:
            for process in reversed(processes):
                stop(process)
                try: os.killpg(process.pid,signal.SIGTERM)
                except ProcessLookupError: pass
Path(output).write_text(json.dumps(result,indent=2)+'\n')
