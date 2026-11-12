local workspace = require('workspace')
local dir = vim.fn.tempname() .. ' with space'
vim.fn.mkdir(dir, 'p')
local cases = {
    { 'c', 'demo.c', '#include <stdio.h>\nint main(void){puts("C_OK");}', 'C_OK' },
    { 'cpp', 'demo.cpp', '#include <iostream>\nint main(){std::cout<<"CPP_OK";}', 'CPP_OK' },
    { 'python', 'demo.py', 'print("PY_OK")', 'PY_OK' },
    { 'javascript', 'demo.js', 'console.log("JS_OK")', 'JS_OK' },
    { 'java', 'Main.java', 'class Main {public static void main(String[] args){System.out.println("JAVA_OK");}}', 'JAVA_OK' },
}
for _, case in ipairs(cases) do
    local file = dir .. '/' .. case[2]
    vim.fn.writefile(vim.split(case[3], '\n'), file)
    local command, temporary = workspace.command(file, case[1])
    local result = vim.system(command, { text = true }):wait()
    assert(result.code == 0 and result.stdout:find(case[4], 1, true), vim.inspect(result))
    if temporary then vim.fn.delete(temporary) end
end
assert(workspace.command(dir .. '/x.json', 'json') == nil)
assert(workspace.command(dir .. '/x.css', 'css') == nil)
vim.fn.writefile({ '<h1>ok</h1>' }, dir .. '/index.html')
assert(workspace.command(dir .. '/x.css', 'css')[4] == dir .. '/index.html')
local opener = dir .. '/xdg-open'
local marker = dir .. '/preview-url'
vim.fn.writefile({ '#!/bin/sh', 'printf "%s" "$1" > "$NVIM_PREVIEW_URL"' }, opener)
vim.fn.setfperm(opener, 'rwx------')
local preview = vim.system(workspace.command(dir .. '/index.html', 'html'), {
    env = { PATH = dir .. ':' .. vim.env.PATH, NVIM_PREVIEW_URL = marker },
})
assert(vim.wait(5000, function() return vim.fn.filereadable(marker) == 1 end, 50), 'preview não abriu URL')
local url = table.concat(vim.fn.readfile(marker), '')
assert(url:match('^http://127%.0%.0%.1:'), 'preview expôs endereço indevido')
local page = vim.system({ 'curl', '-fsS', url }, { text = true }):wait()
preview:kill(2)
preview:wait()
assert(page.code == 0 and page.stdout:find('<h1>ok</h1>', 1, true), 'HTML não foi servido')
vim.fn.delete(dir, 'rf')
print('Runner: C/C++, Python, JavaScript, Java, espaços no caminho e CSS passaram.')
