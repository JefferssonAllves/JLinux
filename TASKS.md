# Índice de trabalho

## NEOVIM-WORKSPACE — implementado e instalado

Perfil independente do desktop em config/nvim: árvore esquerda, terminal inferior, abas, execução por F5, tema Shades of Purple e LSP/completar das linguagens principais. Instalado no usuário jlinux em 12/11/2026, Neovim 0.12.6, JDK 27, clang 23 e ferramentas Arch; Java LSP pelo Mason. Atalho Neovim Code no menu. Sem mudança nos binários/serviços do desktop.

Evidência: runner (cinco linguagens, C/C++ separados, HTML local e CSS), sete inicializações LSP e detecção automática, UI RPC com execução por F5 e layout, desktop entry validado. Documentação e comandos em config/nvim/README.md. Sem CI para estes testes pessoais dependentes das ferramentas instaladas; sem benchmark ou equivalência visual exata ao VS Code. Configuração enviada em branch para revisão; instalação pessoal ativa, sem merge autorizado.

## NEOVIM-DARK-BORDERS — implementado e instalado

Variação escura do perfil de #85, com painéis profundos, separadores violeta, contornos azul-claro e cabeçalhos sublinhados. Sem dependências novas. UI RPC verificou cores, contornos arredondados, persistência após recarga, layout e botão Executar. Backup da configuração anterior preservado; aplicação completa na próxima abertura do editor. Sem comparação visual física ou efeitos gráficos do compositor. Branch dependente de feature/neovim-workspace; sem merge autorizado.

## NEOVIM-BOLD-NEON — implementado e instalado

Atalho Neovim Code em FiraCode Nerd Font Mono Bold 12 pt, fundo escuro preservado, bordas e destaques neon reforçados. Base dependente de #86. UI RPC (cores, bold, recarga, layout e botão), Fontconfig, Foot e desktop entry validados. Backup anterior preservado; fonte nova na próxima abertura pelo menu. Sem mudança global na fonte dos outros terminais nem comparação visual física.

## NEOVIM-TREE-POLISH — implementado e instalado

Mockup detalhado em `docs/design/neovim-workspace-preview.png`. Marcadores de status Git ocultos; bordas discretas azul-ardósia/violeta; texto mais claro e em negrito; árvore reduzida a 26 colunas; removidos “Explorador”, barras extras e botão visual, mantendo execução por F5. Atalho `a` cria arquivos e pastas pelo caminho digitado. Backup do perfil anterior em `~/.config/nvim.before-preview-20261112-2045`. UI RPC passou verificando cores, ausência do título/marcadores `?`, layout e execução por F5.

## NEOVIM-DEVELOPMENT — implementado e instalado

Baseado no layout vigente de feature/neovim-tree-polish (#89). Ctrl+D seleciona ocorrências, Ctrl+; comenta, atalhos cobrem busca/clipboard/undo/mover/duplicar/LSP/formatação. Autosave nativo ~1 s e ao sair do arquivo, com proteção para readonly, nomes ausentes e alteração externa. Busca sob demanda, LSP por filetype, dois plugins novos sem atualizar os anteriores. Pacotes fzf, ruff e shfmt instalados.

Evidência: sugestões reais em 14 tipos (schemas locais JSON/YAML e módulos Go/Rust), UI confirmou edição múltipla, comentários, temporização/proteções, Ctrl+P, Ctrl+Espaço/Enter e formatação Python. Regressões de layout/criação/F5 e runner passaram. Backup e guia em config/nvim/README.md. Sem benchmark de memória/CPU, suporte universal a linguagens, depurador novo ou encerramento da sessão. Ativação na próxima abertura do editor; PR para revisão sem merge.

## Botão executar no nvimcode — feature/neovim-run-button

Componente clicável discreto na lualine, condicionado ao executor existente e ao arquivo atual. Reutiliza workspace.run/F5. Configuração instalada em ~/.config/nvim; backup anterior em /tmp/jlinux-nvim-init-before-run-button.lua. Sintaxe Lua e carregamento headless conferidos. Instâncias abertas precisam reabrir para carregar a configuração.

## Mensagens do executor nvimcode

Início com nome do arquivo e término com código de saída em ANSI rosa-violeta/negrito. Configuração aplicada ao usuário, preservando backup em /tmp/jlinux-workspace-before-messages.lua. Execução de teste com saída 7 validou ambas as mensagens e preservação do código.

## Terminal de origem do nvimcode

Iniciador e função Bash instalados no usuário: shell sem outros jobs é substituído por lançador destacado, preservando fonte Bold 12 e app-id existentes. Terminal integrado e shells com jobs são preservados. Sintaxe shell/Bash validada; fechamento real da janela ativa não executado para preservar a sessão de trabalho.
