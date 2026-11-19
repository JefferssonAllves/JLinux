#!/usr/bin/env python3
"""Minimize a live terminal, preserve it, restore focus and geometry."""
import os,re,runpy,subprocess,sys,tempfile,time
from pathlib import Path
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait,stop=h['wait_for'],h['stop']
compositor,foot,wtype,pointer=[str(Path(x).resolve()) for x in sys.argv[1:5]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-minimize-') as directory:
 root=Path(directory);logfile=root/'log';marker=root/'restored'
 env=dict(os.environ,XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER=os.environ.get('WLR_RENDERER','pixman'),WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
 def log():return logfile.read_text(errors='replace')
 def keys(*args):subprocess.run([wtype,'-s','100',*args],env=env,check=True,timeout=5)
 try:
  with logfile.open('w') as out:
   server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
   wait(lambda:'JLinux compositor pronto:' in log(),processes,'compositor')
   env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
   one=subprocess.Popen([foot,'--config=/dev/null','--term=xterm-256color','--title=Minimize-1','sh'],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(one)
   pattern=r'Janela mapeada: Minimize-1 \((\d+)x(\d+) em (\d+),(\d+)\)'
   wait(lambda:re.search(pattern,log()),processes,'terminal')
   w,ht,x,y=map(int,re.search(pattern,log()).groups());time.sleep(.25)
   two=subprocess.Popen([foot,'--config=/dev/null','--term=xterm-256color','--title=Minimize-2','sh'],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(two)
   wait(lambda:'Janela mapeada: Minimize-2' in log(),processes,'second terminal')
   keys('-M','logo','-k','m','-m','logo')
   wait(lambda:'Janela minimizada: Minimize-2' in log(),processes,'shortcut minimizes')
   time.sleep(.25);assert two.poll() is None and 'Janela desmapeada: Minimize-2' not in log()
   keys('-M','alt','-k','Tab','-m','alt')
   wait(lambda:'Janela restaurada: Minimize-2' in log(),processes,'Alt+Tab restores')
   keys('-M','alt','-k','Tab','-m','alt');time.sleep(.25)
   subprocess.run([pointer,str(x+w-97),str(y-18)],env=env,check=True,timeout=5)
   wait(lambda:'Janela minimizada: Minimize-1' in log(),processes,'titlebar minimizes')
   time.sleep(.25);assert one.poll() is None
   keys('-M','alt','-k','Tab','-m','alt')
   wait(lambda:'Janela restaurada: Minimize-1' in log(),processes,'restore clicked window')
   time.sleep(.25);keys(f"printf restored > '{marker}'",'-k','Return')
   wait(lambda:marker.exists(),processes,'restored terminal receives keyboard')
   assert marker.read_text()=='restored'
   keys('-M','logo','-k','m','-m','logo');time.sleep(.25)
   keys('-M','logo','-k','q','-m','logo')
   wait(lambda:two.poll() is not None,processes[:2],'close remaining visible terminal')
   keys('-M','logo','-k','Tab','-m','logo')
   wait(lambda:'Visão de janelas aberta: 1 janelas' in log(),processes[:2],'overview includes minimized window')
   keys('-k','Return')
   wait(lambda:log().count('Janela restaurada: Minimize-1')>=2,processes[:2],'overview restores minimized window')
   if len(sys.argv)>5:
    fixture=subprocess.Popen([str(Path(sys.argv[5]).resolve())],env=env,stdin=subprocess.PIPE,stdout=out,stderr=out,text=True);processes.append(fixture)
    fixture.stdin.write('create 0 -1 0 0 0 0\n');fixture.stdin.flush()
    wait(lambda:'Janela mapeada: Layout-0' in log(),[server,fixture],'protocol fixture')
    fixture.stdin.write('minimize 0 -1 0 0 0 0\n');fixture.stdin.flush()
    wait(lambda:'Janela minimizada: Layout-0' in log(),[server,fixture],'xdg set_minimized request')
    keys('-M','alt','-k','Tab','-m','alt')
    wait(lambda:'Janela restaurada: Layout-0' in log(),[server,fixture],'restore protocol request')
 except Exception:
  print(log(),file=sys.stderr);raise
 finally:
  for process in reversed(processes):stop(process)
