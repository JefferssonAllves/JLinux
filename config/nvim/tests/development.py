"""Real key input, simultaneous edits, comments and native idle autosave in embedded UI."""
import logging
logging.disable(logging.CRITICAL)
from pathlib import Path
import tempfile
import time
import pynvim
n = pynvim.attach('child', argv=['nvim', '--embed'])
def keys(value):
    n.input(value)
    time.sleep(.15)
def text(): return list(n.current.buffer[:])
try:
    n.ui_attach(140, 45, rgb=True)
    n.exec_lua('''
        _G.test_clipboard = {{}, 'v'}
        local copy = function(lines, kind) _G.test_clipboard = {lines, kind} end
        local paste = function() return _G.test_clipboard end
        vim.g.clipboard = {name='test-only', copy={['+']=copy,['*']=copy}, paste={['+']=paste,['*']=paste}}
    ''')
    n.exec_lua('vim.wait(3000, function() return #vim.api.nvim_list_wins() >= 3 end)')
    with tempfile.TemporaryDirectory(prefix='nvim-development-') as directory:
        file = Path(directory) / 'main.py'
        file.write_text('alpha = 1\nprint(alpha)\nalphabet = 2\nalpha = 3\n')
        n.command('edit ' + n.funcs.fnameescape(str(file)))
        n.current.window.cursor = (1, 1)
        keys('<C-d>')
        assert len(n.eval('b:VM_Selection.Regions')) == 3, 'Ctrl+D did not select all whole-word matches'
        keys('cbeta<Esc><Esc>')
        assert text() == ['beta = 1', 'print(beta)', 'alphabet = 2', 'beta = 3'], text()
        n.current.window.cursor = (1, 2)
        keys('i<C-d>cgamma<Esc><Esc>')
        assert text() == ['gamma = 1', 'print(gamma)', 'alphabet = 2', 'gamma = 3'], text()
        n.current.window.cursor = (1, 0)
        keys('<C-;>')
        assert text()[0].startswith('#'), text()
        keys('<C-;>')
        assert text()[0] == 'gamma = 1', text()
        keys('i<C-;><Esc>')
        assert text()[0].startswith('#'), text()
        keys('<C-;>ggVj<C-;>')
        assert all(line.startswith('#') for line in text()[:2]), text()
        keys('<Esc>')
        # Isolate the idle timer from earlier edits and events.
        n.command('enew!')
        file = Path(directory) / 'autosave.py'
        file.write_text('value = 1\n')
        n.command('edit ' + n.funcs.fnameescape(str(file)))
        keys('A # idle')
        time.sleep(.35)
        assert file.read_text() == 'value = 1\n', 'saved before roughly one second'
        time.sleep(1.1)
        assert file.read_text() == 'value = 1 # idle\n', 'idle insert did not autosave'
        keys(' # local')
        file.write_text('external change\n')
        time.sleep(1.35)
        assert file.read_text() == 'external change\n', 'overwrote an external modification'
        assert n.current.buffer.options['modified'], 'local edits were discarded'
        keys('<Esc>')
        n.command('edit!')
        n.current.buffer.options['readonly'] = True
        n.current.buffer[:] = ['readonly local change']
        n.command('doautocmd CursorHold')
        assert file.read_text() == 'external change\n', 'readonly file autosaved'
        n.current.buffer.options['readonly'] = False
        n.command('edit!')
        keys('A # leave<Esc>')
        n.command('enew')
        assert file.read_text() == 'external change # leave\n', 'buffer leave did not save'
        keys('iuntitled')
        time.sleep(1.3)
        assert n.current.buffer.name == '' and n.current.buffer.options['modified'], 'unnamed buffer was written'
        keys('<Esc>')
        n.command('enew!')
        n.current.buffer[:] = ['one', 'two']
        n.current.window.cursor = (1, 0)
        keys('<A-Down>')
        assert text() == ['two', 'one'], text()
        keys('<A-S-Down>')
        assert text() == ['two', 'one', 'one'], text()
        keys('<C-z>')
        assert text() == ['two', 'one'], text()
        keys('<C-y>')
        assert text() == ['two', 'one', 'one'], text()
        keys('ggV<C-c>G<C-v>')
        assert text()[-1] == 'two', 'clipboard copy/paste failed'
        target = Path(directory) / 'search-target.py'
        target.write_text('print("search")\n')
        n.command('cd ' + n.funcs.fnameescape(directory))
        assert n.exec_lua("return package.loaded['fzf-lua'] == nil"), 'search loaded without being requested'
        keys('<C-p>')
        time.sleep(.4)
        keys('search-target<CR>')
        time.sleep(.4)
        assert n.current.buffer.name == str(target), 'Ctrl+P did not open selected file'
        completion = Path(directory) / 'completion.py'
        completion.write_text('class Item:\n    value = 1\nitem = Item()\nitem.va\n')
        n.command('edit ' + n.funcs.fnameescape(str(completion)))
        assert n.exec_lua("return vim.wait(15000,function() for _,c in ipairs(vim.lsp.get_clients({bufnr=0,name='pyright'})) do if c.initialized then return true end end return false end,100)"), 'pyright did not initialize in UI'
        n.current.window.cursor = (4, 6)
        keys('A<C-space>')
        assert n.exec_lua("return vim.wait(8000,function() local cmp=require('blink.cmp'); if not cmp.is_visible() then return false end; for _,item in ipairs(cmp.get_items()) do if item.label=='value' and item.source_id=='lsp' then return true end end; return false end,100)"), 'Ctrl+Space did not show real LSP completion'
        keys('<CR>')
        assert text()[-1] == 'item.value', 'Enter did not accept completion'
        keys('<Esc>')

        assert n.exec_lua("return vim.wait(15000,function() for _,c in ipairs(vim.lsp.get_clients({bufnr=0,name='ruff'})) do if c.initialized then return true end end return false end,100)"), 'ruff did not initialize'
        n.current.buffer[:] = ['numbers=[1,2,3]']
        keys('<A-S-f>')
        assert n.exec_lua("return vim.wait(5000,function() return vim.api.nvim_get_current_line() == 'numbers = [1, 2, 3]' end,100)"), 'format shortcut did not format Python'
    print('Development: Ctrl+D normal/insert, simultaneous editing, comments, idle autosave, external/readonly/unnamed guards, move/duplicate/undo/redo lazy search real Ctrl+Space/Enter LSP completion and Python formatting passed.')
finally:
    try: n.command('qa!')
    except (EOFError, pynvim.api.common.NvimError): pass
