local dir = vim.fn.tempname()
vim.fn.mkdir(dir .. '/.git', 'p')
local schema = {type='object',properties={active={type='boolean'}}}
local schema_file = dir..'/schema.json'
vim.fn.writefile({vim.json.encode(schema)},schema_file)
vim.lsp.config('jsonls',{settings={json={schemas={{fileMatch={'config.json'},schema=schema}}}}})
vim.lsp.config('yamlls',{settings={yaml={schemaStore={enable=false},schemas={[vim.uri_from_fname(schema_file)]='config.yaml'}}}})
vim.fn.mkdir(dir..'/go','p')
vim.fn.writefile({'module example.test/local','go 1.27'},dir..'/go/go.mod')
vim.fn.mkdir(dir..'/rust/src','p')
vim.fn.writefile({'[package]','name="test_completion"','version="0.1.0"','edition="2021"'},dir..'/rust/Cargo.toml')
local cases = {
    { 'config.json', 'json', 'jsonls', '{"act', 'active' },
    { 'config.yaml', 'yaml', 'yamlls', 'act', 'active' },
    { 'go/main.go', 'go', 'gopls', 'package main\nimport "fmt"\nfunc main(){fmt.Pr', 'Print' },
    { 'rust/src/main.rs', 'rust', 'rust_analyzer', 'struct Item {value:i32}\nfn main(){let item=Item{value:1}; item.', 'value' },
    { 'main.c', 'c', 'clangd', 'struct Item {int value;};\nint main(void){struct Item item; item.', 'value' },
    { 'main.cpp', 'cpp', 'clangd', 'struct Item {int value;};\nint main(){Item item; item.', 'value' },
    { 'main.py', 'python', 'pyright', 'class Item:\n    value = 1\nitem = Item()\nitem.', 'value' },
    { 'main.js', 'javascript', 'ts_ls', 'const item = {value:1};\nitem.', 'value' },
    { 'main.ts', 'typescript', 'ts_ls', 'const item: {value:number} = {value:1};\nitem.', 'value' },
    { 'index.html', 'html', 'html', '<div cla', 'class' },
    { 'main.css', 'css', 'cssls', 'body { col', 'color' },
    { 'Main.java', 'java', 'jdtls', 'class Main {public static void main(String[] args){String value=""; value.', 'length' },
    { 'main.lua', 'lua', 'lua_ls', 'local item = {value=1}\nitem.', 'value' },
    { 'main.sh', 'sh', 'bashls', 'example_function() { :; }\nexample_f', 'example_function' },
}
for _, case in ipairs(cases) do
    local file = dir .. '/' .. case[1]
    local lines = vim.split(case[4], '\n')
    vim.fn.writefile(lines, file)
    vim.cmd.edit(vim.fn.fnameescape(file))
    assert(vim.bo.filetype == case[2], 'filetype: ' .. case[1])
    local client
    assert(vim.wait(45000, function()
        for _, candidate in ipairs(vim.lsp.get_clients({bufnr=0,name=case[3]})) do
            if candidate.initialized then client=candidate; return true end
        end
        return false
    end,100), 'LSP não iniciou: '..case[3])
    assert(client.config.capabilities.textDocument.completion.completionItem.snippetSupport)
    local params = {textDocument={uri=vim.uri_from_fname(file)}, position={line=#lines-1, character=#lines[#lines]}}
    local found = false
    for _=1,20 do
        local response = client:request_sync('textDocument/completion',params,5000,0)
        if response and response.result then
            local items = response.result.items or response.result
            for _, item in ipairs(items) do
                if item.label and item.label:find(case[5],1,true) then found=true;break end
            end
        end
        if found then break end
        vim.wait(300)
    end
    assert(found,'Sugestão esperada não recebida: '..case[1]..' -> '..case[5])
    print(case[1]..': IntelliSense real '..case[5]..' OK')
    for _, running in ipairs(vim.lsp.get_clients()) do running:stop(true) end
end
vim.fn.delete(dir,'rf')
