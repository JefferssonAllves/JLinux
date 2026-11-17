"""Click the real statusline button, autosave HTML, then stop the local preview."""
import logging
import os
from pathlib import Path
import tempfile
import time
import urllib.error
import urllib.request
import pynvim
logging.disable(logging.CRITICAL)
config=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='nvim live ') as temp:
    root=Path(temp);file=root/'preview page.html';file.write_text('<h1>original</h1>')
    opener=root/'xdg-open';marker=root/'url'
    opener.write_text('#!/bin/sh\nprintf "%s" "$1" > "$NVIM_PREVIEW_URL"\n');opener.chmod(0o700)
    old_path=os.environ['PATH'];os.environ['PATH']=str(root)+':'+old_path;os.environ['NVIM_PREVIEW_URL']=str(marker)
    profile=root/'config';profile.mkdir();(profile/'nvim').symlink_to(config,target_is_directory=True)
    old_config=os.environ.get('XDG_CONFIG_HOME');os.environ['XDG_CONFIG_HOME']=str(profile)
    n=pynvim.attach('child',argv=['nvim','--embed'])
    os.environ['PATH']=old_path;os.environ.pop('NVIM_PREVIEW_URL',None)
    if old_config is None:os.environ.pop('XDG_CONFIG_HOME',None)
    else:os.environ['XDG_CONFIG_HOME']=old_config
    try:
        n.ui_attach(140,45,rgb=True)
        assert n.exec_lua('return vim.wait(4000,function() return #vim.api.nvim_list_wins()>=3 end)')
        editor=next(win for win in n.windows if win.buffer.options['buftype']=='' and win.buffer.options['filetype']!='NvimTree')
        n.api.set_current_win(editor.handle);n.command('edit '+n.funcs.fnameescape(str(file)))
        html=n.current.buffer.number
        assert Path(n.exec_lua('return require("workspace").command(...,"html")[2]',str(file))).resolve()==config/'preview.py'
        def click_live():
            n.command('redrawstatus')
            position=n.exec_lua('''local r=vim.o.lines-vim.o.cmdheight;for c=1,vim.o.columns-3 do
                if vim.fn.screenstring(r,c)=='L' and vim.fn.screenstring(r,c+1)=='i' and vim.fn.screenstring(r,c+2)=='v' and vim.fn.screenstring(r,c+3)=='e' then return {r-1,c-1} end end''')
            assert position,'Live button is missing from the statusline'
            n.api.input_mouse('left','press','',0,*position);n.api.input_mouse('left','release','',0,*position)
        click_live()
        deadline=time.monotonic()+6
        while not marker.exists() and time.monotonic()<deadline:time.sleep(.05)
        assert marker.exists(),'Click did not start preview'
        assert n.current.buffer.number==html,'Preview stole focus from the HTML editor'
        url=marker.read_text();assert url.startswith('http://127.0.0.1:')
        n.input('gg0C<h1>autosaved</h1>')
        deadline=time.monotonic()+7
        while 'autosaved' not in file.read_text() and time.monotonic()<deadline:time.sleep(.1)
        assert 'autosaved' in file.read_text(),'Autosave stopped while preview was running'
        assert b'autosaved' in urllib.request.urlopen(url,timeout=3).read()
        n.input('\x1b');time.sleep(.1);click_live()
        assert n.exec_lua('return vim.wait(4000,function() return not require("workspace").preview_running() end)')
        try:urllib.request.urlopen(url,timeout=1);raise AssertionError('Stop did not close preview port')
        except (urllib.error.URLError, ConnectionResetError):pass
        print('UI: real Live button starts/stops preview, preserves editor focus and HTML autosave.')
    finally:
        try:n.command('qa!')
        except (EOFError,pynvim.api.common.NvimError):pass
