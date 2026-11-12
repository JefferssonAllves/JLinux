vim.g.mapleader = ' '
vim.g.loaded_netrw = 1
vim.g.loaded_netrwPlugin = 1
if vim.fn.argc() > 0 then
    local path = vim.fn.fnamemodify(vim.fn.argv(0), ':p')
    local directory = vim.fn.isdirectory(path) == 1 and path or vim.fs.dirname(path)
    vim.api.nvim_set_current_dir(vim.fs.root(directory, { '.git', 'package.json', 'pom.xml', 'pyproject.toml', 'CMakeLists.txt', 'go.mod' }) or directory)
end
vim.opt.termguicolors = true
vim.opt.mouse = 'a'
vim.opt.number = true
vim.opt.signcolumn = 'yes'
vim.opt.expandtab = true
vim.opt.shiftwidth = 4
vim.opt.tabstop = 4
vim.opt.splitbelow = true
vim.opt.splitright = true
vim.opt.ignorecase = true
vim.opt.smartcase = true
vim.opt.clipboard = 'unnamedplus'
vim.opt.updatetime = 250
vim.opt.laststatus = 3
vim.opt.confirm = true
vim.opt.undofile = true
vim.opt.completeopt = { 'menu', 'menuone', 'noselect' }
local lazy = vim.fn.stdpath('data') .. '/lazy/lazy.nvim'
if not vim.uv.fs_stat(lazy) then
    vim.fn.system({ 'git', 'clone', '--filter=blob:none', '--branch=stable', 'https://github.com/folke/lazy.nvim.git', lazy })
    assert(vim.v.shell_error == 0, 'Não foi possível instalar lazy.nvim')
end
vim.opt.rtp:prepend(lazy)
require('lazy').setup({
    { 'necrogoru/shades-of-purple.nvim', priority = 1000, config = function() vim.cmd.colorscheme('shades-of-purple') end },
    { 'nvim-tree/nvim-web-devicons' },
    { 'nvim-tree/nvim-tree.lua', opts = { hijack_directories = { enable = false }, view = { width = 30 }, update_focused_file = { enable = true }, filters = { dotfiles = false } } },
    { 'akinsho/bufferline.nvim', version = '*', opts = { options = { diagnostics = 'nvim_lsp', offsets = { { filetype = 'NvimTree', text = 'EXPLORADOR', text_align = 'left' } } } } },
    { 'nvim-lualine/lualine.nvim', opts = { options = { theme = 'auto', globalstatus = true }, sections = { lualine_c = { { 'filename', path = 1 } }, lualine_x = { 'diagnostics', 'filetype' } } } },
    { 'saghen/blink.cmp', version = '1.*', opts = { keymap = { preset = 'super-tab' }, fuzzy = { implementation = 'lua' }, completion = { documentation = { auto_show = true } }, signature = { enabled = true } } },
    { 'mason-org/mason.nvim', opts = {} },
    { 'neovim/nvim-lspconfig', config = function()
        local capabilities = require('blink.cmp').get_lsp_capabilities()
        for _, server in ipairs({ 'clangd', 'pyright', 'ts_ls', 'html', 'cssls', 'jsonls', 'yamlls', 'lua_ls', 'bashls', 'gopls', 'rust_analyzer', 'jdtls' }) do
            vim.lsp.config(server, { capabilities = capabilities })
            vim.lsp.enable(server)
        end
    end },
}, { checker = { enabled = false }, change_detection = { notify = false } })
vim.diagnostic.config({ virtual_text = true, underline = true, severity_sort = true })
require('workspace').setup()
