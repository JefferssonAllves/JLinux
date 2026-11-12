# Índice de trabalho

## NEOVIM-WORKSPACE — implementado e instalado

Perfil independente do desktop em config/nvim: árvore esquerda, terminal inferior, abas, botão clicável/F5, tema Shades of Purple e LSP/completar das linguagens principais. Instalado no usuário jlinux em 12/11/2026, Neovim 0.12.6, JDK 27, clang 23 e ferramentas Arch; Java LSP pelo Mason. Atalho Neovim Code no menu. Sem mudança nos binários/serviços do desktop.

Evidência: runner (cinco linguagens, C/C++ separados, HTML local e CSS), sete inicializações LSP e detecção automática, UI RPC com clique real no botão e layout, desktop entry validado. Documentação e comandos em config/nvim/README.md. Sem CI para estes testes pessoais dependentes das ferramentas instaladas; sem benchmark ou equivalência visual exata ao VS Code. Configuração enviada em branch para revisão; instalação pessoal ativa, sem merge autorizado.

## NEOVIM-DARK-BORDERS — implementado e instalado

Variação escura do perfil de #85, com painéis profundos, separadores violeta, contornos azul-claro e cabeçalhos sublinhados. Sem dependências novas. UI RPC verificou cores, contornos arredondados, persistência após recarga, layout e botão Executar. Backup da configuração anterior preservado; aplicação completa na próxima abertura do editor. Sem comparação visual física ou efeitos gráficos do compositor. Branch dependente de feature/neovim-workspace; sem merge autorizado.

## NEOVIM-BOLD-NEON — implementado e instalado

Atalho Neovim Code em FiraCode Nerd Font Mono Bold 12 pt, fundo escuro preservado, bordas e destaques neon reforçados. Base dependente de #86. UI RPC (cores, bold, recarga, layout e botão), Fontconfig, Foot e desktop entry validados. Backup anterior preservado; fonte nova na próxima abertura pelo menu. Sem mudança global na fonte dos outros terminais nem comparação visual física.
