local M = {}
function M.setup()
    local colors = require('shades-of-purple.colors')
    colors.bg = '#171126'
    colors.bg_dark = '#100C1D'
    colors.bg_darker = '#0C0917'
    colors.bg_lighter = '#211832'
    colors.border = '#C77DFF'
    colors.border_highlight = '#43F6FF'
    colors.purple = '#DA7CFF'
    colors.purple_bright = '#E49AFF'
    colors.pink = '#FF66CE'
    colors.cyan = '#43F6FF'
    colors.green_light = '#8CFF9C'
    colors.yellow = '#FFF066'
    vim.opt.winborder = 'rounded'
    vim.opt.fillchars:append({ vert = '│', horiz = '─', horizup = '┴', horizdown = '┬', vertleft = '┤', vertright = '├', verthoriz = '┼' })
    local function borders()
        local groups = {
            Normal = { fg = colors.fg, bg = colors.bg, bold = true },
            NormalNC = { fg = colors.fg, bg = colors.bg, bold = true },
            WinSeparator = { fg = '#44364E', bg = colors.bg },
            FloatBorder = { fg = colors.border_highlight, bg = colors.bg_dark },
            WinBar = { fg = '#F4E6FF', bg = colors.bg_dark, sp = colors.border, underline = true },
            WinBarNC = { fg = '#BAA8D8', bg = colors.bg_dark, sp = '#63468C', underline = true },
            NvimTreeWinSeparator = { fg = '#344451', bg = colors.bg_dark },
            NvimTreeNormal = { fg = '#FFF2FF', bg = colors.bg_dark, bold = true },
            NvimTreeNormalNC = { fg = '#FFF2FF', bg = colors.bg_dark, bold = true },
            NvimTreeFolderName = { fg = '#E49AFF', bg = colors.bg_dark, bold = true },
            NvimTreeOpenedFolderName = { fg = '#F2C0FF', bg = colors.bg_dark, bold = true },
            NvimTreeEmptyFolderName = { fg = '#E49AFF', bg = colors.bg_dark, bold = true },
            NvimTreeRootFolder = { fg = '#43F6FF', bg = colors.bg_dark, bold = true },
            NvimTreeExecFile = { fg = '#A6FFB4', bg = colors.bg_dark, bold = true },
            NvimTreeSpecialFile = { fg = '#FFF082', bg = colors.bg_dark, bold = true },
            NvimTreeSymlink = { fg = '#73E9FF', bg = colors.bg_dark, bold = true },
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
