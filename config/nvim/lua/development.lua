local M = {}
local function disk_signature(file)
    local s = vim.uv.fs_stat(file)
    if not s then return 'missing' end
    return table.concat({ s.dev, s.ino, s.size, s.mtime.sec, s.mtime.nsec, s.ctime.sec, s.ctime.nsec }, ':')
end
local function remember(event)
    local b = event.buf
    vim.b[b].autosave_signature = disk_signature(vim.api.nvim_buf_get_name(b))
    vim.b[b].autosave_warning = nil
end
function M.save(buffer)
    if not vim.api.nvim_buf_is_valid(buffer) then return end
    local options = vim.bo[buffer]
    local file = vim.api.nvim_buf_get_name(buffer)
    if options.buftype ~= '' or not options.modified or not options.modifiable or options.readonly or file == '' then return end
    local baseline = vim.b[buffer].autosave_signature
    if not baseline or baseline ~= disk_signature(file) then
        if not vim.b[buffer].autosave_warning then
            vim.notify('Autosave suspenso: arquivo alterado fora do editor ou renomeado. Revise antes de salvar.', vim.log.levels.WARN)
            vim.b[buffer].autosave_warning = true
        end
        return
    end
    local ok, error = pcall(vim.api.nvim_buf_call, buffer, function() vim.cmd('silent update') end)
    if not ok then vim.notify('Autosave: ' .. tostring(error), vim.log.levels.ERROR) end
end
function M.setup()
    -- Native idle events avoid another plugin, timer loop or file watcher.
    vim.opt.updatetime = 1000
    local group = vim.api.nvim_create_augroup('JLinuxDevelopment', { clear = true })
    vim.api.nvim_create_autocmd({ 'BufReadPost', 'BufNewFile', 'BufWritePost' }, { group = group, callback = remember })
    vim.api.nvim_create_autocmd('BufFilePost', { group = group, callback = function(e) vim.b[e.buf].autosave_signature = nil end })
    vim.api.nvim_create_autocmd({ 'CursorHold', 'CursorHoldI', 'BufLeave', 'FocusLost' }, { group = group, callback = function(e) M.save(e.buf) end })
    for _, buffer in ipairs(vim.api.nvim_list_bufs()) do remember({ buf = buffer }) end
    local map = vim.keymap.set
    map('i', '<C-d>', '<Esc><Plug>(VM-Select-All)', { remap = true, desc = 'Selecionar todas as ocorrências' })
    for _, key in ipairs({ '<C-;>', '<C-/>', '<C-_>' }) do
        map('n', key, 'gcc', { remap = true, desc = 'Comentar linha' })
        map('x', key, 'gc', { remap = true, desc = 'Comentar seleção' })
        map('i', key, '<Esc>gccgi', { remap = true, desc = 'Comentar linha' })
    end
    map({ 'n', 'i' }, '<C-a>', '<Esc>ggVG', { desc = 'Selecionar arquivo inteiro' })
    map('n', '<C-c>', '"+yy', { desc = 'Copiar linha' })
    map('i', '<C-c>', '<C-o>"+yy', { desc = 'Copiar linha' })
    map('n', '<C-x>', '"+dd', { desc = 'Recortar linha' })
    map('i', '<C-x>', '<C-o>"+dd', { desc = 'Recortar linha' })
    map('x', '<C-c>', '"+y', { desc = 'Copiar' })
    map('x', '<C-x>', '"+d', { desc = 'Recortar' })
    map('n', '<C-v>', '"+p', { desc = 'Colar' })
    map('i', '<C-v>', '<C-r>+', { desc = 'Colar' })
    map({ 'n', 'i' }, '<C-z>', '<Cmd>undo<CR>', { desc = 'Desfazer' })
    map({ 'n', 'i' }, '<C-y>', '<Cmd>redo<CR>', { desc = 'Refazer' })
    map({ 'n', 'i' }, '<C-S-z>', '<Cmd>redo<CR>', { desc = 'Refazer' })
    map({ 'n', 'i' }, '<C-f>', '<Esc>/', { desc = 'Buscar no arquivo' })
    map('n', '<F3>', 'n', { desc = 'Próximo resultado' })
    map('n', '<S-F3>', 'N', { desc = 'Resultado anterior' })
    map({ 'n', 'i' }, '<A-Up>', '<Cmd>move .-2<CR>', { desc = 'Mover linha acima' })
    map({ 'n', 'i' }, '<A-Down>', '<Cmd>move .+1<CR>', { desc = 'Mover linha abaixo' })
    map({ 'n', 'i' }, '<A-S-Down>', '<Cmd>copy .<CR>', { desc = 'Duplicar linha' })
    map({ 'n', 'i' }, '<A-S-Up>', '<Cmd>copy .-1<CR>', { desc = 'Duplicar linha acima' })
    map({ 'n', 'i' }, '<C-S-f>', function() require('fzf-lua').live_grep() end, { desc = 'Buscar no projeto' })
    map({ 'n', 'i' }, '<C-p>', function() require('fzf-lua').files() end, { desc = 'Abrir arquivo' })
    map({ 'n', 'i' }, '<C-S-p>', function() require('fzf-lua').commands() end, { desc = 'Paleta de comandos' })
    map({ 'n', 'i' }, '<F12>', vim.lsp.buf.definition, { desc = 'Ir à definição' })
    map({ 'n', 'i' }, '<S-F12>', vim.lsp.buf.references, { desc = 'Referências' })
    map({ 'n', 'i' }, '<A-CR>', vim.lsp.buf.code_action, { desc = 'Ações de código' })
    map({ 'n', 'i' }, '<A-S-f>', function() vim.lsp.buf.format({ async = true }) end, { desc = 'Formatar com LSP' })
    map('n', '<F8>', function() vim.diagnostic.jump({ count = 1, float = true }) end, { desc = 'Próximo diagnóstico' })
    map('n', '<S-F8>', function() vim.diagnostic.jump({ count = -1, float = true }) end, { desc = 'Diagnóstico anterior' })
end
return M
