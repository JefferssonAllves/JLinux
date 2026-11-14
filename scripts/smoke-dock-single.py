#!/usr/bin/env python3
"""Existing/minimized/off-workspace windows are activated; pending launches coalesce."""
import os,runpy,socket,subprocess,sys,tempfile,time
from pathlib import Path
h=runpy.run_path(str(Path(__file__).with_name('smoke-shell.py')));wait,stop=h['wait_for'],h['stop']
compositor,foot,wtype=[str(Path(x).resolve()) for x in sys.argv[1:4]]
processes=[]
with tempfile.TemporaryDirectory(prefix='jlinux-dock-single-') as directory:
 root=Path(directory);logfile=root/'log';commands=root/'commands';commands.mkdir();calls=root/'calls'
 mock=commands/'foot';mock.write_text(f'#!{sys.executable}\nfrom pathlib import Path\nwith Path({str(calls)!r}).open("a") as f:f.write("launch\\n")\n');mock.chmod(0o755)
 config=root/'config';config.mkdir();(root/'Pictures').mkdir();(root/'Videos').mkdir()
 (config/'user-dirs.dirs').write_text(f'XDG_PICTURES_DIR="{root}/Pictures"\nXDG_VIDEOS_DIR="{root}/Videos"\n')
 apps_dir=root/'data/applications';apps_dir.mkdir(parents=True)
 file_mock=commands/'files-test';file_calls=root/'file-calls'
 file_mock.write_text(f'#!{sys.executable}\nimport sys\nfrom pathlib import Path\nwith Path({str(file_calls)!r}).open("a") as f:f.write(sys.argv[-1]+"\\n")\n');file_mock.chmod(0o755)
 (apps_dir/'org.jlinux.Files.desktop').write_text(f'[Desktop Entry]\nType=Application\nName=Files test\nExec={file_mock} %f\n')
 env=dict(os.environ,PATH=str(commands)+os.pathsep+os.environ['PATH'],XDG_CONFIG_HOME=str(config),XDG_DATA_HOME=str(root/'data'),XDG_RUNTIME_DIR=directory,WLR_BACKENDS='headless',WLR_RENDERER='pixman',WLR_HEADLESS_OUTPUTS='1',JLINUX_TEST_INPUT='1')
 def log():return logfile.read_text(errors='replace')
 def request(index):
  with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as s:s.sendto(bytes([index]),str(root/'jlinux-window-control'))
 def keys(*args):subprocess.run([wtype,'-s','100',*args],env=env,check=True,timeout=5)
 try:
  with logfile.open('w') as out:
   server=subprocess.Popen([compositor],env=env,stdout=out,stderr=out);processes.append(server)
   wait(lambda:'JLinux compositor pronto:' in log(),processes,'ready')
   env['WAYLAND_DISPLAY']=next(p.name for p in root.glob('wayland-*') if p.is_socket())
   assert (root/'jlinux-window-control').stat().st_mode&0o777==0o600
   apps=((0,'brave-browser'),(1,'org.jlinux.Terminal'),(2,'org.jlinux.Neovim'),(3,'org.qbittorrent.qBittorrent'),(4,'org.jlinux.Files'),(8,'org.jlinux.Monitor'),(9,'org-spotube-spotube-MainKt'),(10,'brave-web.whatsapp.com__-Default'))
   for index,appid in apps:
    title=f'Dock-{index}'
    client=subprocess.Popen([foot,'--config=/dev/null','--app-id='+appid,'--title='+title,'sh'],env=env,stdout=out,stderr=out);processes.append(client)
    wait(lambda:f'Janela mapeada: {title}' in log(),processes,'map')
    before=log().count('Aplicativo existente focado:');request(index);request(index)
    wait(lambda:log().count('Aplicativo existente focado:')>=before+2,processes,'reuse existing window')
    if index==4:
     before=log().count('Aplicativo existente focado:');request(6);request(7)
     wait(lambda:log().count('Aplicativo existente focado:')>=before+2,processes,'folder shortcuts share Files')
    if index==2:
     keys('-M','logo','-k','m','-m','logo');wait(lambda:f'Janela minimizada: {title}' in log(),processes,'minimize')
     time.sleep(.2);request(index);wait(lambda:f'Janela restaurada: {title}' in log(),processes,'dock restores minimized')
     keys('-M','logo','-k','2','-m','logo');before=log().count('Aplicativo existente focado:');request(index)
     wait(lambda:log().count('Aplicativo existente focado:')>before,processes,'activate across workspaces')
     assert client.poll() is None
    stop(client);processes.remove(client)
   assert not calls.exists(),'existing windows caused extra launches'
   for _ in range(5):request(1)
   wait(lambda:calls.exists(),processes,'launch when no window')
   time.sleep(.25);assert calls.read_text().splitlines()==['launch'],'rapid clicks launched duplicates'
   request(8);wait(lambda:len(calls.read_text().splitlines())==2,processes,'independent app launch')
   request(6);request(7);request(4)
   wait(lambda:file_calls.exists(),processes,'folder launch when Files is closed')
   time.sleep(.2);assert file_calls.read_text().splitlines()==[str(root/'Pictures')],'Files launch parameters or folder coalescing failed'
   with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as s:
    for command in (b'',b'\xff',b'\x05',b'\x01\x01'):s.sendto(command,str(root/'jlinux-window-control'))
   time.sleep(.1);assert server.poll() is None
 except Exception:
  print(log(),file=sys.stderr);raise
 finally:
  for process in reversed(processes):stop(process)
