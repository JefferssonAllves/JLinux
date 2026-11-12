import pynvim, json, tempfile, sys, logging
logging.disable(logging.CRITICAL)
from pathlib import Path
n = pynvim.attach('child', argv=['nvim', '--embed', *sys.argv[1:]])
try:
    n.ui_attach(140, 45, rgb=True)
    n.exec_lua('vim.wait(3000, function() return #vim.api.nvim_list_wins() >= 3 end)')
    windows = [dict(pos=n.api.win_get_position(w), width=w.width, height=w.height, ft=w.buffer.options['filetype'], kind=w.buffer.options['buftype']) for w in n.windows]
    print(json.dumps(windows))
    assert any(w['ft']=='NvimTree' and w['pos'][1]==0 for w in windows), 'arvore ausente'
    assert any(w['kind']=='terminal' and w['pos'][0]>0 for w in windows), 'terminal ausente'
    assert n.eval('g:colors_name') == 'shades-of-purple'
    assert n.api.get_hl(0, {'name': 'Normal'})['bg'] == 0x171126
    assert n.api.get_hl(0, {'name': 'WinSeparator'})['fg'] == 0x9874E8
    assert n.api.get_hl(0, {'name': 'FloatBorder'})['fg'] == 0x6FD7FF
    assert n.options['winborder'] == 'rounded'
    n.command('colorscheme shades-of-purple')
    assert n.api.get_hl(0, {'name': 'RunButton'})['underline'], 'borda nao sobreviveu a recarga'
    text = n.exec_lua("local lines = {}; for r=1,45 do local line=''; for c=1,140 do line=line..vim.fn.screenstring(r,c) end; lines[#lines+1]=line end; return table.concat(lines,'\\n')")
    open('/tmp/jlinux-neovim-screen.txt','w').write(text)
    assert 'Executar' in text, 'botao ausente'
    assert text.count('Executar') == 1, 'botao apareceu fora do editor'
    with tempfile.TemporaryDirectory(prefix='nvim-button-') as directory:
        file = Path(directory) / 'demo.py'
        file.write_text('print("RUN_BUTTON_OK")\n')
        n.command('edit ' + n.funcs.fnameescape(str(file)))
        n.command('redraw!')
        # Editor winbar in the single-grid UI, above the code and at the right edge.
        n.api.input_mouse('left', 'press', '', 0, 1, 130)
        n.api.input_mouse('left', 'release', '', 0, 1, 130)
        assert n.exec_lua("return vim.wait(10000, function() for _, b in ipairs(vim.api.nvim_list_bufs()) do if vim.bo[b].buftype == 'terminal' then for _, line in ipairs(vim.api.nvim_buf_get_lines(b,0,-1,false)) do if line == 'RUN_BUTTON_OK' then return true end end end end; return false end, 100)"), 'clique no botao nao executou'
    print('UI: arvore esquerda, terminal inferior, tema e clique Executar OK')
finally:
    try:n.command('qa!')
    except (EOFError, pynvim.api.common.NvimError):pass
