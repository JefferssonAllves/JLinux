local M = {}
local terminal_win, shell_buf, run_buf, run_job
local function bottom(buffer)
    if not terminal_win or not vim.api.nvim_win_is_valid(terminal_win) then
        if vim.bo.buftype ~= '' then
            for _, win in ipairs(vim.api.nvim_list_wins()) do
                if vim.bo[vim.api.nvim_win_get_buf(win)].buftype == '' then
                    vim.api.nvim_set_current_win(win)
                    break
                end
            end
        end
        vim.cmd('belowright 12split')
        terminal_win = vim.api.nvim_get_current_win()
    end
    vim.api.nvim_set_current_win(terminal_win)
    vim.api.nvim_win_set_buf(terminal_win, buffer)
    vim.wo.winbar = ' TERMINAL'
    vim.wo.number = false
    vim.wo.signcolumn = 'no'
end
function M.terminal()
    if terminal_win and vim.api.nvim_win_is_valid(terminal_win) then
        vim.api.nvim_win_close(terminal_win, false)
        terminal_win = nil
        return
    end
    local previous = vim.api.nvim_get_current_win()
    if not shell_buf or not vim.api.nvim_buf_is_valid(shell_buf) then
        shell_buf = vim.api.nvim_create_buf(false, true)
        bottom(shell_buf)
        vim.fn.jobstart({ vim.o.shell }, { term = true })
    else
        bottom(shell_buf)
    end
    vim.api.nvim_set_current_win(previous)
end
local html_preview = [[
import http.server, os, sys, subprocess, urllib.parse
os.chdir(os.path.dirname(sys.argv[1]))
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), http.server.SimpleHTTPRequestHandler)
url = 'http://127.0.0.1:%s/%s' % (server.server_port, urllib.parse.quote(os.path.basename(sys.argv[1])))
print(url, flush=True)
subprocess.Popen(['xdg-open', url])
server.serve_forever()
]]
function M.command(file, filetype)
    local commands = {
        python = { 'python', file }, java = { 'java', file },
        javascript = { 'node', file }, typescript = { 'node', file },
        sh = { 'bash', file }, lua = { 'lua', file }, go = { 'go', 'run', file },
        html = { 'python', '-c', html_preview, file },
    }
    if filetype == 'c' or filetype == 'cpp' then
        local output = vim.fn.tempname()
        return { 'sh', '-c', (filetype == 'c' and 'gcc' or 'g++') .. ' -Wall -Wextra -g "$2" -o "$1" && exec "$1"', 'nvim-run', output, file }, output
    end
    if filetype == 'css' then
        local index = vim.fs.dirname(file) .. '/index.html'
        if vim.fn.filereadable(index) == 1 then return { 'python', '-c', html_preview, index } end
    end
    return commands[filetype]
end
function M.run()
    if run_job and vim.fn.jobwait({ run_job }, 0)[1] == -1 then
        vim.notify('Há uma execução ativa. Use Ctrl+C no terminal para encerrá-la.', vim.log.levels.WARN)
        return
    end
    local file = vim.api.nvim_buf_get_name(0)
    if vim.bo.buftype ~= '' or file == '' then
        vim.notify('Abra e salve um arquivo de código antes de executar.', vim.log.levels.WARN)
        return
    end
    vim.cmd.write()
    local command, temporary = M.command(file, vim.bo.filetype)
    if not command then
        vim.notify('Este tipo de arquivo não é executável sozinho. Para CSS, abra um HTML ou crie index.html na mesma pasta.', vim.log.levels.WARN)
        return
    end
    if vim.fn.executable(command[1]) ~= 1 then
        vim.notify('Ferramenta não instalada: ' .. command[1], vim.log.levels.ERROR)
        return
    end
    if run_buf and vim.api.nvim_buf_is_valid(run_buf) then vim.api.nvim_buf_delete(run_buf, { force = true }) end
    local buffer = vim.api.nvim_create_buf(false, true)
    run_buf = buffer
    bottom(buffer)
    run_job = vim.fn.jobstart(command, { term = true, cwd = vim.fs.dirname(file), on_exit = function()
        if temporary then vim.fn.delete(temporary) end
    end })
    vim.cmd.startinsert()
end
function M.setup()
    _G.JLinuxRun = function()
        local win = vim.fn.getmousepos().winid
        if vim.api.nvim_win_is_valid(win) then vim.api.nvim_set_current_win(win) end
        M.run()
    end
    local toolbar = '%#WinBar# %f %m%=%#RunButton#%@v:lua.JLinuxRun@ ▶ Executar %X'
    _G.JLinuxWinbar = function()
        if vim.bo.filetype == 'NvimTree' then return '' end
        if vim.bo.buftype == 'terminal' then return ' TERMINAL' end
        return toolbar
    end
    vim.opt.winbar = '%{%v:lua.JLinuxWinbar()%}'
    vim.keymap.set({ 'n', 'i' }, '<C-s>', '<Cmd>write<CR>')
    vim.keymap.set({ 'n', 'i' }, '<F5>', M.run)
    vim.keymap.set('n', '<C-b>', '<Cmd>NvimTreeToggle<CR>')
    vim.keymap.set({ 'n', 'i', 't' }, '<C-`>', M.terminal)
    vim.keymap.set('n', '<leader>tt', M.terminal)
    vim.keymap.set('t', '<Esc>', '<C-\\><C-n>')
    vim.keymap.set('n', '<S-l>', '<Cmd>bnext<CR>')
    vim.keymap.set('n', '<S-h>', '<Cmd>bprevious<CR>')
    for key, direction in pairs({ h = 'h', j = 'j', k = 'k', l = 'l' }) do
        vim.keymap.set({ 'n', 't' }, '<C-' .. key .. '>', '<Cmd>wincmd ' .. direction .. '<CR>')
    end
    vim.api.nvim_create_autocmd('LspAttach', { callback = function(event)
        local opts = { buffer = event.buf }
        vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
        vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
        vim.keymap.set('n', '<F2>', vim.lsp.buf.rename, opts)
        vim.keymap.set('n', '<leader>ca', vim.lsp.buf.code_action, opts)
    end })
    vim.api.nvim_create_autocmd('VimEnter', { once = true, callback = function()
        if #vim.api.nvim_list_uis() == 0 then return end
        if vim.fn.isdirectory(vim.api.nvim_buf_get_name(0)) == 1 then vim.cmd.enew() end
        local editor = vim.api.nvim_get_current_win()
        require('nvim-tree.api').tree.open()
        vim.api.nvim_set_current_win(editor)
        M.terminal()
    end })
end
return M
