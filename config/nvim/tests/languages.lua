local dir = vim.fn.tempname()
vim.fn.mkdir(dir .. '/.git', 'p')
local cases = {
    { 'main.c', 'c', 'clangd', 'int main(void){return 0;}' },
    { 'main.cpp', 'cpp', 'clangd', 'int main(){return 0;}' },
    { 'main.py', 'python', 'pyright', 'print("ok")' },
    { 'main.js', 'javascript', 'ts_ls', 'console.log("ok")' },
    { 'index.html', 'html', 'html', '<html><body>ok</body></html>' },
    { 'main.css', 'css', 'cssls', 'body { color: red; }' },
    { 'Main.java', 'java', 'jdtls', 'class Main {public static void main(String[] args){}}' },
}
for _, case in ipairs(cases) do
    local file = dir .. '/' .. case[1]
    vim.fn.writefile({ case[4] }, file)
    vim.cmd.edit(vim.fn.fnameescape(file))
    assert(vim.bo.filetype == case[2], 'filetype incorreto: ' .. case[1])
    assert(vim.wait(45000, function()
        for _, client in ipairs(vim.lsp.get_clients({ bufnr = 0, name = case[3] })) do
            if client.initialized then return true end
        end
        return false
    end, 100), 'LSP não iniciou: ' .. case[3])
    print(case[1] .. ': detecção e ' .. case[3] .. ' OK')
    for _, client in ipairs(vim.lsp.get_clients()) do client:stop(true) end
end
vim.fn.delete(dir, 'rf')
