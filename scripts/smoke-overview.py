#!/usr/bin/env python3
"""Exercise overview blur, pointer/keyboard selection, pages, removal and input isolation."""
import os
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile
import time
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')))
wait,stop=h['wait_for'],h['stop']
compositor,fixture,wtype,pointer,shell=[str(Path(a).resolve()) for a in sys.argv[1:6]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-overview-') as directory:
 root=Path(directory)
 env=dict(os.environ,XDG_RUNTIME_DIR=directory,XDG_CONFIG_HOME=str(root/'config'),WLR_BACKENDS='headless',WLR_RENDERER=os.environ.get('WLR_RENDERER','pixman'),WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
 server_log=root/'server.log';client_log=root/'client.log'
 def log():return server_log.read_text(errors='replace')
 def command(name,index):client.stdin.write(f'{name} {index} -1 0 0 0 0\n');client.stdin.flush()
 def keys(*args):subprocess.run([wtype,'-s','100',*args],env=env,check=True,timeout=5)
 def show():
  before=log().count('Visão de janelas aberta:')
  super_key='Super_R' if before%2 else 'Super_L'
  keys('-k',super_key,'-s','80','-k',super_key)
  wait(lambda:log().count('Visão de janelas aberta:')>before,processes,'overview opens')
  if env.get('JLINUX_GLASS_BLUR')!='0':assert re.findall(r'desfoque=([01])',log())[-1]=='1','real blur capture failed'
 def active(index):
  flags=re.findall(rf'^active {index} ([01])$',client_log.read_text(),re.M)
  return flags and flags[-1]=='1'
 try:
  with server_log.open('w') as out,client_log.open('w') as events:
   server=subprocess.Popen([compositor],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(server)
   wait(lambda:'JLinux compositor pronto:' in log(),processes,'socket')
   env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
   panel=subprocess.Popen([shell],env=env,stdout=out,stderr=subprocess.STDOUT);processes.append(panel)
   wait(lambda:'Interface mapeada: jlinux-panel' in log(),processes,'shell')
   client=subprocess.Popen([fixture],env=env,stdin=subprocess.PIPE,stdout=events,stderr=out,text=True);processes.append(client)
   for i in range(10):
    command('create',i);wait(lambda i=i:f'Janela mapeada: Layout-{i} ' in log(),processes,'map')
   time.sleep(.25)
   before=log().count('Visão de janelas aberta:')
   keys('-M','logo','-k','Tab','-m','logo')
   assert log().count('Visão de janelas aberta:')==before,'old Super+Tab still opens overview'
   keys('-k','Super_L','-M','logo','-k','Tab','-m','logo','-s','80','-k','Super_L')
   assert log().count('Visão de janelas aberta:')==before,'a Super shortcut failed to cancel the primed tap'
   keys('-k','Super_L','-s','500','-k','Super_L')
   assert log().count('Visão de janelas aberta:')==before,'slow taps triggered overview'
   keys('-P','Super_L','-s','300','-p','Super_L','-s','80','-k','Super_L')
   assert log().count('Visão de janelas aberta:')==before,'holding Super counted as a tap'
   show()
   assert '10 janelas' in log()
   closed_before=log().count('Visão de janelas fechada')
   subprocess.run([wtype,'-k','Escape'],env=env,check=True,timeout=4)
   wait(lambda:log().count('Visão de janelas fechada')>closed_before,processes,'cancel opening fade')
   show()
   closed_before=log().count('Visão de janelas fechada')
   keys('-k','Super_L','-s','80','-k','Super_L')
   wait(lambda:log().count('Visão de janelas fechada')>closed_before,processes,'double Super toggles overview closed')
   show()
   captured=log().count('Visão de janelas aberta:')
   readbacks=log().count('Fundo da visão capturado:')
   if env.get('JLINUX_GLASS_BLUR')=='0':assert readbacks==0
   inputs=len(re.findall(r'^key ',client_log.read_text(),re.M))
   focus_events=client_log.read_text().count('keyboard-focus')
   keys('x')
   assert len(re.findall(r'^key ',client_log.read_text(),re.M))==inputs,'overview keys leaked to the client'
   assert client_log.read_text().count('keyboard-focus')==focus_events,'new keyboard restored focus behind overview'
   keys('-k','Page_Down');wait(lambda:'Página de janelas: 2' in log(),processes,'page 2')
   subprocess.run([pointer,'--scroll','640','390','-15'],env=env,check=True,timeout=4)
   wait(lambda:log().count('Página de janelas: 1')>=2,processes,'wheel returns to first page')
   assert log().count('Visão de janelas aberta:')==captured
   assert log().count('Fundo da visão capturado:')==readbacks,'paging recaptured the background'
   keys('-k','Escape')
   show()
   # MRU order 9..0. First card at 32,96 with 392x184 dimensions in a 3x3 grid.
   subprocess.run([pointer,'180','180'],env=env,check=True,timeout=4)
   wait(lambda:active(9),processes,'click focuses selected window')
   show()
   keys('-k','Right','-k','Return');wait(lambda:active(8),processes,'keyboard selection')
   show()
   # A disappearing client must close/release thumbnails safely.
   closed=log().count('Visão de janelas fechada')
   command('unmap',7);wait(lambda:log().count('Visão de janelas fechada')>closed,processes,'unmap closes overview')
   show()
   pages_before=log().count('Página de janelas:')
   subprocess.run([pointer,'--scroll','640','390','15'],env=env,check=True,timeout=4)
   assert log().count('Página de janelas:')==pages_before,'single page was unnecessarily rebuilt'
   stop(client);processes.remove(client)
   show()  # No windows remains usable and dismissible.
   keys('-k','Escape')
   stop(panel);processes.remove(panel)
   stop(server);processes.remove(server)
   assert server.returncode==0
   costs=re.findall(r'preparação=([0-9.]+) ms',log())
   print('Overview preparation ms (headless/'+env['WLR_RENDERER']+'):',', '.join(costs))
   for line in log().splitlines():
    if 'GL renderer:' in line or 'EGL vendor:' in line: print(line)
 except Exception:
  print(log(),file=sys.stderr);print(client_log.read_text() if client_log.exists() else '',file=sys.stderr);raise
 finally:
  for p in reversed(processes):stop(p)
print('Overview: real blur, click/keyboard, paging, disappearance and empty state passed.')
