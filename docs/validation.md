# Validação inicial

Em 04/11/2026, após instalar GCC, Meson, Ninja, pkgconf, GTK3 e gtk-layer-shell no ambiente:

- `meson setup build --buildtype=release`: passou.
- `meson compile -C build`: passou, warnings tratados como erros.
- `DESTDIR=/tmp/jlinux-install-check meson install -C build`: passou; binário e configuração instalados no staging.
- `sh -n scripts/collect-hardware.sh`: passou.
- Coleta de hardware pelo script: passou.
- `git diff --check`: passou.

Versões: GCC 16.2.1, Meson 1.12.1, GTK 3.24.52 e gtk-layer-shell 0.10.1.

Ainda pendentes: execução gráfica em Sway, validação da configuração pelo compositor, consumo de memória/CPU/energia e testes de interação. O workflow será executado somente após publicação no GitHub.

## Compositor próprio (0.2.0)

A arquitetura passou a usar o executável jlinux-compositor com wlroots 0.20.2 e libwayland 1.26.0. Sway deixou de fazer parte da configuração e do build.

- Compilação nativa em release, com warnings tratados como erros: passou.
- Smoke test headless/Pixman: passou; saída ativa, socket disponível, encerramento por SIGTERM com status zero e remoção do socket.
- O teste cobre ciclo de vida do servidor. Não cobre clientes Wayland, janelas, entrada, renderização em GPU ou personalização.

A configuração TOML é somente proposta. Nenhuma animação, efeito ou adaptação dinâmica foi implementada nesta etapa.

## Interface mínima (0.3.0)

- `meson setup build-desktop --buildtype=release -Dshell=true`, compilação e dois testes: passaram.
- Shell headless: GTK mapeou wallpaper e painel no compositor próprio. O teste encerrou/reiniciou o cliente duas vezes, manteve o compositor vivo, rejeitou tema CSS inválido e verificou encerramento limpo do servidor e remoção do socket.
- Build separado `build-sanitize` com `-Db_sanitize=address,undefined -Db_lundef=false`: compilação e os dois testes passaram. Bibliotecas do sistema não foram recompiladas com instrumentação. O cliente é encerrado por sinal; este teste não prova ausência de vazamentos do toolkit.
- `scripts/run-desktop.py --headless --duration 1 --theme config/jlinux/theme.css`: passou.
- Instalação em staging: passou. Build exclusivo de compositor com `-Dshell=false` e smoke test: passaram.

O suporte layer-shell inicial configura tamanho/posição e camadas, fecha layers quando sua saída é destruída e permite reiniciar o shell. Não implementa entrada, popups, política para janelas xdg, bloqueio ou portais. Não houve inspeção visual em monitor físico, teste aninhado com GPU ou benchmark; somente mapeamento headless. O relógio atualiza a cada 30 segundos; não há efeitos ou animações contínuas.

## Terminal e entrada (0.4.0)

Release e ASan/UBSan: os três testes passaram. Novo teste abre dois Foot reais, digita comandos que escrevem uma identificação em arquivo temporário, troca foco com cliente de ponteiro virtual, fecha a janela focada e confirma entrada na outra; encerra o compositor por Super+Esc. Introduz espera de sincronização para criação dos recursos de entrada dos clientes antes de injetar eventos.

Runner headless com terminal e duração limitada: passou. Execução curta DRM/libinput por quatro segundos: passou sem queda dos processos. Não equivale a teste manual de teclado/mouse ou inspeção visual. A execução persistente solicitada pelo usuário será verificada pelos logs de dispositivos e mapeamento antes da entrega. Não há benchmark nesta etapa.

Foot e wtype são binários do sistema, não instrumentados no build ASan. O teste injeta entrada somente quando JLINUX_TEST_INPUT=1; a sessão normal não publica os protocolos de entrada virtual. Bibliotecas do sistema continuam fora da instrumentação.

### Execução persistente no hardware

Em 05/11/2026, após CI verde do PR #6, a sessão foi iniciada por `run-desktop.py --drm --codex`, sem temporizador. Logs confirmaram renderização GLES2 pela Radeon/radeonsi, saída HDMI-A-1 ativa, teclado USB/interno e mouse/touchpad conectados. Wallpaper, painel e terminal foram mapeados. A árvore de processos confirmou `foot` com `codex` como filho; Codex CLI estava autenticado com ChatGPT.

Isso confirma inicialização e detecção dos periféricos no hardware real, sem afirmar teste manual de movimento/digitação pelo usuário, inspeção visual ou benchmark. Logs locais de supervisão ficam em `~/.local/state/jlinux/desktop.log`; a sessão pode ser encerrada por Super+Esc. A inicialização não instalou serviço ou entrada de login.

## Separação do pipeline visual

Criação/limpeza de renderizador e alocador wlroots e apresentação/frame-done foram extraídas para src/renderer/renderer.c. Release e ASan/UBSan passaram nos três testes existentes: ciclo de vida, reinício do shell e entrada/foco em terminais reais no backend headless. O TOML v2 foi analisado por tomllib sem erro de sintaxe.

animation/ e effects/ contêm contratos documentados; ainda não há motores, shaders, toggles funcionais ou mudança visual Glass. Esta etapa não mediu desempenho nem reiniciou a sessão gráfica do usuário para teste físico.

## Shell e iniciador nativos — 0.5

O shell GTK/CSS foi substituído por cliente Wayland/C com painel, dock, ponteiro, tipografia Pango e buffers SHM desenhados com Cairo. O iniciador jlinux-session também é C; o script Python de execução e o tema CSS foram removidos. Python permanece nos testes.

- Build release: passou com warnings como erros.
- Quatro testes headless: passaram; cobrem ciclo de vida, shell/dock/wallpaper e clique que abre terminal, reinício do shell, teclado/foco em terminais e iniciador C com timeout/SIGTERM/validação de duração e limpeza do runtime.
- Build ASan/UBSan: os mesmos quatro testes passaram, com verificação do status zero do shell no encerramento. Caches de fontes são liberados no shutdown; não foram usadas supressões ou desativação de leak detection.
- Instalação em staging: passou para compositor, shell e iniciador.
- `ldd jlinux-shell`: sem GTK ou interpretador Python.
- PNG de preview: gerado e inspecionado; usa as rotinas nativas de desenho, não uma captura de DRM.

O backend do toolkit ainda rasteriza buffers em CPU, com apresentação pelo compositor. Não houve benchmark ou comparação de eficiência. Não há blur/refração Glass, parser de tema, integração Wi-Fi/Bluetooth/energia ou fidelidade visual completa às referências. Dados e ações ainda indisponíveis são explicitados pelo shell.

## Material inicial do shell — feature/glass-material

Seis testes passaram em build-native (release) e build-native-asan (address,undefined), sem supressões: ciclo de compositor/shell/sessão, terminal/entrada, blur ARGB32 e oito combinações independentes de blur/tint/reflection. Prévia nativa inspecionada em docs/design/native-glass-preview.png. Blur usa wallpaper reduzido, não conteúdo de janelas; não valida fidelidade às referências ou posicionamento em múltiplos monitores.

Medição exploratória única no ambiente local: execução completa de `jlinux-shell --preview /tmp/jlinux-cost.png`, via `time.monotonic` e `resource.getrusage(RUSAGE_CHILDREN)`: 0,335 s e pico RSS 27.836 KiB. Inclui inicialização, fontes, rasterização e escrita PNG; não é tempo de quadro, benchmark comparável ou medição de GPU/energia. Cache de fundo 1600×900 usa imagem 400×225 ARGB32 (360.000 bytes, além de scratch temporário durante blur). Custos sustentados da sessão e adaptação continuam pendentes.

### Ajuste de contraste do Glass — fix/glass-contrast

Seis testes locais passaram novamente em release e ASan/UBSan após alterar tonalização, reflexos estáticos e opacidade dos botões. As oito combinações dos controles continuam distintas e válidas. Prévia nativa atualizada e inspecionada; wallpaper preservado. Não houve benchmark de GPU/energia nem confirmação de aprovação pelo usuário.

## Molduras nativas e reflexos localizados — feature/window-glass-frames

Oito testes locais release e ASan/UBSan passaram, sem supressões. Novos testes validam recriação/liberação dos quatro buffers, limites de tamanho, opacidade por foco e quantidade de pixels alocados; teste de entrada com Foot real arrasta pelo cabeçalho, fecha pelo botão e redimensiona pela borda inferior/direita, conferindo mudança de linhas/colunas via stty. Testes anteriores de foco, desconexão, terminal, shell e sessão continuam passando. Negociação de decoração foi ajustada para aguardar inicialização do xdg_surface antes do configure.

Prévias de moldura e barra inspecionadas. Não há blur nas molduras ou máscara arredondada do conteúdo dos aplicativos. Sem benchmark GPU/energia nesta entrega. Testes headless não comprovam comportamento físico completo.

Sessão aninhada iniciada automaticamente usando jlinux-session, mantendo o compositor DRM e terminais anteriores. Logs locais registram backend Wayland, GLES2/Mesa na AMD Radeon e terminal/painel/dock mapeados. Isso confirma inicialização aninhada com GPU, não inspeção visual da sessão ou benchmark. O shell da sessão externa também foi atualizado para os pontos de brilho. Molduras do compositor antigo não podem ser atualizadas ao vivo; a avaliação das molduras novas ocorre na sessão aninhada.

### Aplicação na sessão DRM principal

Após autorização explícita do usuário para reiniciar, a sessão principal antiga e a prévia aninhada foram encerradas. Nova sessão iniciada com `jlinux-session --drm --codex`, supervisionada pelo iniciador C. Logs confirmam renderizador AMD Radeon/GLES2, saída HDMI-A-1 e mapeamento de Codex, wallpaper, painel e dock. As molduras agora pertencem ao compositor da sessão principal. Essa evidência confirma inicialização/mapeamento; aprovação visual e interação física pelo usuário permanecem pendentes.

## Maximização/restauração — feature/window-maximize

Oito testes locais passaram em release e ASan/UBSan. Foot real valida maximizar por Super+↑ e botão, restaurar pelo botão e Super+↓, preservação do tamanho anterior, pedido inicial `--maximized` e resize posterior. Cabeçalho/fechar/drag continuam cobertos. Teste com shell valida área de conteúdo maximizada 1198×628 em 76,86 dentro de saída headless 1280×720, descontando barra, dock e moldura. Esse teste revelou reserva incorreta antes do map; listeners de map/unmap agora recalculam a área útil.

Prévia de componentes atualizada. Sessão DRM corrente preservada e ainda usa a versão anterior; ativação e avaliação física desta mudança pendentes. Não houve benchmark GPU/energia.

### Ativação da maximização na sessão principal

Usuário autorizou novo reinício da sessão. Supervisor anterior encerrado normalmente e sessão iniciada com a versão feature/window-maximize via `jlinux-session --drm --codex`. Logs confirmam AMD Radeon/GLES2, saída HDMI-A-1 e Codex/wallpaper/painel/dock mapeados. Maximizar/restaurar e a correção da área útil agora estão no compositor da sessão principal; teste físico e avaliação do usuário continuam pendentes.
