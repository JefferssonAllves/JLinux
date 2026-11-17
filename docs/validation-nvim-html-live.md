# NvimCode: botão Live para HTML

Instalado em 16/11/2026 a partir de 1d1e5fe. Branch feature/nvim-html-live dependente de fix/nvimcode-current-terminal (PR #102), preservando também o ajuste de nove linhas do terminal já instalado (c86ba52).

- Servidor local usa Python 3 existente, porta livre em 127.0.0.1, sem plugin novo. HTML servido recebe EventSource; alterações nos arquivos solicitados pelo navegador são verificadas a cada 400 ms. HTML, CSS, JS e outros recursos carregados geram recarga completa. Fonte no disco não recebe script injetado.
- Dois testes HTTP/SSE e Chromium real passaram: HTML/CSS/JS recarregados sem ação manual, fonte preservada, cache desabilitado, Host local e symlinks para fora da raiz bloqueados em GET/HEAD, incluindo index.htm em diretório.
- UI Neovim por RPC: clique real no botão da statusline iniciou e parou a prévia; foco permaneceu no HTML; edição foi salva pelo autosave; HTTP retornou a alteração e o servidor encerrou. Repetido com os arquivos efetivamente instalados. Navegador pessoal substituído por stub apenas no teste de UI; nenhum editor pessoal encerrado.
- Regressões passaram: runners C/C++/Java/Python/JavaScript, caminhos com espaços, CSS/index.html, layout da árvore, tema, terminal de nove linhas e F5 real de Python. git diff --check passou.

Instalação somente de init.lua, lua/workspace.lua, preview.py e README em ~/.config/nvim. Backup completo em ~/.config/nvim.before-live-20261116-213400. Comparação com a configuração prévia confirmou preservação de personalizações e da altura do terminal. Plugins e lazy-lock.json não foram atualizados. Os dois PIDs de Neovim existentes foram preservados; instâncias abertas mantêm os módulos antigos até reabrir. Não foi forçado reload para não perder estado de terminais/jobs.

A prévia serve a pasta do HTML e suas subpastas; referências acima dessa raiz não são servidas. Não executa backends nem substitui bundlers. Uma CSP que proíba scripts inline pode bloquear a recarga, sem remoção da política da página. Não houve teste visual no monitor físico ou benchmark de desempenho.
