# Neovim Code — Shades of Purple

Perfil pessoal instalado em `~/.config/nvim`, Neovim 0.12.6. Abra pelo menu **Neovim Code** (Foot com FiraCode Nerd Font Mono Bold, 12 pt) ou use `nvim` / `nvim pasta-do-projeto`. Não é uma interface gráfica idêntica ao VS Code: mantém os modos e comandos do Neovim.

A árvore abre à esquerda, o editor à direita, e o terminal abaixo do editor. Abas no topo, informações/diagnósticos embaixo e botão **▶ Executar** no canto superior direito do editor. Tema: port [Shades of Purple](https://github.com/necrogoru/shades-of-purple.nvim), referência confirmada pelo usuário.

## Uso

- `i`: editar; `Esc`: voltar ao modo normal. No terminal, `Esc` sai da digitação; `i` volta a digitar nele.
- `Ctrl+S`: salvar. Clique **▶ Executar** ou use `F5`: salva e executa o arquivo atual.
- `Ctrl+B` no modo normal: mostrar/ocultar a árvore. Na árvore, Enter abre; `a` cria arquivo ou pasta (termine o nome da pasta com `/`).
- `Ctrl+crase`: mostrar/ocultar terminal. Alternativa no modo normal: Espaço, `t`, `t`.
- `Ctrl+H/J/K/L`: trocar de painel no modo normal/terminal.
- `Shift+H/L`: arquivo anterior/seguinte no modo normal.
- `gd`: definição; `K`: documentação; `F2`: renomear símbolo; Espaço, `c`, `a`: ações de código, no modo normal.
- `Tab`: completar código/snippets. `:Mason`: ferramentas adicionais. `:Lazy`: plugins.
- `Ctrl+C` dentro do terminal em digitação encerra uma execução/servidor. Não começa outra execução enquanto a anterior estiver ativa.

## Linguagens

Detecção por nome/extensão com sintaxe do Neovim, diagnósticos/LSP e autocompletar via blink.cmp. Servidores ativados conforme o arquivo: C/C++ (clangd), Java (jdtls), Python (pyright), JavaScript/TypeScript (ts_ls), HTML, CSS, JSON, YAML, Lua, Bash, Go e Rust. Java usa jdtls instalado pelo Mason; os demais usam pacotes Arch. Não são todas as linguagens existentes; `:Mason` permite ampliar a instalação/configuração.

Execução simples do arquivo: C/C++ com gcc/g++, Java pelo modo de arquivo-fonte do JDK, Python, JavaScript/TypeScript pelo Node, Lua, Bash e Go. Binários temporários de C/C++ são apagados após a execução. Projetos com vários arquivos, dependências, Maven/Gradle/CMake/npm etc. podem exigir seus comandos próprios no terminal: o botão não substitui o sistema de build do projeto. Depurador com breakpoints não foi configurado.

HTML inicia um servidor **somente em 127.0.0.1**, em porta livre, e abre o navegador; Ctrl+C encerra o servidor. CSS abre `index.html` da mesma pasta quando existe. Arquivos declarativos como JSON/YAML não são programas executáveis. Rust tem LSP; seu compilador não foi instalado por este perfil.

## Reinstalação

Pacotes instalados:

```sh
sudo pacman -S --needed neovim clang jdk-openjdk pyright typescript-language-server \
  vscode-css-languageserver vscode-html-languageserver vscode-json-languageserver \
  lua-language-server bash-language-server yaml-language-server gopls rust-analyzer \
  fd ripgrep ttf-firacode-nerd wl-clipboard
```

Git, Node/npm, Python, GCC/G++, Lua, Go, curl e Foot já estavam presentes. Preserve eventual configuração anterior antes de copiar este diretório para `~/.config/nvim`. Os plugins são fixados em `lazy-lock.json`; `nvim --headless '+Lazy! restore' +qa` instala as revisões registradas. Instale Java LSP com `:MasonInstall jdtls`. Copie `Neovim-Code.desktop` para `~/.local/share/applications/` e execute `update-desktop-database ~/.local/share/applications` para o atalho.

## Validação local (12/11/2026)

```sh
nvim --headless '+lua dofile("config/nvim/tests/runner.lua")' +qa
nvim --headless '+lua dofile("config/nvim/tests/languages.lua")' +qa
python config/nvim/tests/ui.py .
```

O teste de UI requer `python-pynvim` e a configuração já instalada. Foram verificados os compiladores/runners C/C++, Java, Python e JavaScript, caminhos com espaços, a prévia HTTP de HTML sem abrir o navegador pessoal, o caso CSS e tipos não executáveis. O teste de linguagens confirmou filetype e inicialização dos sete servidores principais (C, C++, Java, Python, JS, HTML, CSS). A UI via RPC confirmou árvore à esquerda, terminal abaixo do código, tema, um único botão e execução real por clique. O desktop entry passou na validação. Não foi feito benchmark, comparação visual pixel a pixel com VS Code ou teste de projetos externos complexos. A instalação não inicia nem encerra a sessão do desktop.

## Variação escura com contornos — 12/11/2026

Pedido posterior: roxo mais escuro e bordas inspiradas no desktop. A base Shades of Purple mantém as cores de sintaxe, com fundo do código `#171126` (antes `#2D2B55`), painéis `#100C1D`, fundos profundos `#0C0917` e linhas discretas `#211832`. Divisórias violeta `#9874E8`, contornos de popups azul-claro `#6FD7FF`, sublinhado nos cabeçalhos e botão Executar. Menus do blink, documentação, assinaturas e Mason usam contorno arredondado; separadores nativos mostram linhas e junções.

A implementação fica em lua/appearance.lua e reaplica os destaques ao recarregar o tema. São cores e contornos do terminal, sem blur/refração ou brilho gráfico do compositor. Teste de UI RPC passou com as novas cores, recarga do tema, layout e clique no botão. Configuração anterior preservada em `~/.config/nvim.before-dark-borders-20261112-201324`. Para carregar tudo, feche e abra o Neovim; não é necessário sair do desktop. Não foi feita comparação visual física lado a lado com o sistema.

## Fonte maior e neon — 12/11/2026

O atalho **Neovim Code** agora seleciona FiraCode Nerd Font Mono **Bold, 12 pt** (antes 11 pt). A cor de fundo continua #171126; bordas violeta #C77DFF e ciano #43F6FF, comentários roxo neon, constantes magenta e strings verde #8CFF9C. O texto base também recebe atributo bold no Neovim. São cores sólidas, sem efeito gráfico de glow.

Feche e abra **Neovim Code pelo menu** para carregar a fonte e o tema. Quando iniciado com `nvim` em outro terminal, o tamanho da fonte continua sendo definido por esse terminal. Perfil anterior salvo em `~/.config/nvim.before-bold-neon-20261112-201642`. UI RPC validou fundo, bold, cores neon, recarga do tema, layout e execução pelo botão; Fontconfig confirmou a variante Bold, e Foot/desktop entry validaram a configuração. Sem avaliação visual física ou teste pixel a pixel.

## Árvore sem status Git e bordas ciano/violeta

A árvore não exibe os marcadores de status Git que apareciam como `?`. A borda da árvore fica ciano e as demais divisórias violeta; nomes de arquivos e pastas aparecem em tons mais claros e negrito. O cabeçalho “Explorador” foi removido. Para criar, pressione `a` na árvore: digite um nome de arquivo, ou termine o caminho da pasta com `/` e confirme com Enter. Alterações entram em vigor ao reabrir o Neovim.
