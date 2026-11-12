local M = {}
function M.setup()
    local colors = require('shades-of-purple.colors')
    colors.bg = '#171126'
    colors.bg_dark = '#100C1D'
    colors.bg_darker = '#0C0917'
    colors.bg_lighter = '#211832'
    colors.border = '#9874E8'
    colors.border_highlight = '#6FD7FF'
    vim.opt.winborder = 'rounded'
    vim.opt.fillchars:append({ vert = '│', horiz = '─', horizup = '┴', horizdown = '┬', vertleft = '┤', vertright = '├', verthoriz = '┼' })
    local function borders()
        local groups = {
            WinSeparator = { fg = colors.border },
            FloatBorder = { fg = colors.border_highlight, bg = colors.bg_dark },
            WinBar = { fg = '#E8DEFF', bg = colors.bg_dark, sp = colors.border, underline = true },
            WinBarNC = { fg = '#BAA8D8', bg = colors.bg_dark, sp = '#63468C', underline = true },
            RunButton = { fg = '#A5FF90', bg = colors.bg_dark, sp = colors.border_highlight, bold = true, underline = true },
            NvimTreeWinSeparator = { fg = colors.border, bg = colors.bg_dark },
            BlinkCmpMenuBorder = { fg = colors.border_highlight, bg = colors.bg_dark },
            BlinkCmpDocBorder = { fg = colors.border_highlight, bg = colors.bg_dark },
            BlinkCmpSignatureHelpBorder = { fg = colors.border_highlight, bg = colors.bg_dark },
        }
        for name, highlight in pairs(groups) do vim.api.nvim_set_hl(0, name, highlight) end
    end
    vim.api.nvim_create_autocmd('ColorScheme', { pattern = 'shades-of-purple', callback = borders })
    vim.cmd.colorscheme('shades-of-purple')
end
return M
