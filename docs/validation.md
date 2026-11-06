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

## Reflexos adicionais das janelas — fix/window-glass-depth

Oito testes release e ASan/UBSan passaram. Teste de moldura adicional confirma que desativar REFLECTION remove o brilho extra (comparação de alpha em ponto iluminado), mantendo a tonalização. Prévia nativa inspecionada. Nova versão aplicada por reinício autorizado da sessão DRM; Codex/painel/dock mapeados. Sem benchmark ou validação visual física pelo assistente. Blur/refração nas molduras permanecem pendentes.

## Acabamento contínuo das bordas — fix/window-dock-material

Oito testes release e ASan/UBSan passaram. Gradiente de cores compartilhado com o dock; decoração usa faixa contínua no lugar dos pontos extras de luz. Prévia de componente inspecionada. Sessão DRM reiniciada com autorização vigente e Codex/painel/dock mapeados. Sem benchmark, blur nas molduras ou aprovação visual final do usuário.

## Bordas finas e pontos intensos — fix/thin-window-highlights

Oito testes release e ASan/UBSan passaram após reduzir bordas para 3 px e remover a faixa larga. Teste do componente verifica contraste de alpha entre o centro de um brilho e trecho discreto, além da desativação de REFLECTION. Testes de maximização adaptados à nova moldura: conteúdo 1274×681 em 3,36 sem shell; 1204×631 em 73,86 com barra/dock, na saída simulada 1280×720. Prévia nativa inspecionada e atualizada. Sessão DRM reiniciada usando autorização vigente; Codex/painel/dock mapeados. Sem benchmark ou aprovação visual final; blur nas molduras segue pendente.

## Sombra interna e contorno superior fino — fix/window-rim-shading

Oito testes locais release e ASan/UBSan passaram. Novos asserts verificam sombreamento independente com REFLECTION desligado; preferência SHADOW inválida é rejeitada. Prévias de janela/barra inspecionadas. Nenhum benchmark ou novo reinício DRM nesta etapa: usuário solicitou preparar instalação/inicialização real no Arch.

## Login próprio e instalação Arch — feature/arch-session-install

Onze testes locais release e ASan/UBSan passaram. Novos testes: autenticação IPC com servidor simulado (prompts secretos/visíveis, info, recusa/cancelamento, resposta inválida/oversize/desconexão); greeter headless e recusa sem GREETD_SOCK; fluxo real de digitação de usuário/senha fictícios na UI, pedido de sessão e ausência da senha nos logs. Isso não valida PAM físico. CI do commit 45c6752 passou com dez testes, antes da ampliação do teste de digitação.

Pacote Arch `jlinux-desktop-git 0.5.0.r33.g45c6752-1` compilado a partir do GitHub com arch-meson/makepkg, dez testes aprovados no build de pacote, instalado pelo pacman. Não houve release/tag. `pacman -Qo` confirmou propriedade de compositor/greeter/entrada Wayland; binários root:root 0755. Staging, validação desktop-file/TOML, shell syntax e systemd-analyze verify passaram. O binário instalado também iniciou sessão headless com limpeza e gerou a prévia do login.

Configurador executado: greetd habilitado (alias display-manager), graphical.target padrão, TTY2 habilitado/ativo. Backup em `/var/lib/jlinux-login-backup.qsemGPVh`. Serviço greetd permanece inativo nesta sessão para não encerrar o TTY1 que sustenta o trabalho; será iniciado no próximo boot. Nenhuma regra PAM foi alterada, nenhum autologin configurado. Primeiro boot/login com senha e troca física de VT permanecem pendentes.

Sessão principal anterior encerrada conforme autorização vigente e iniciada com `/usr/bin/jlinux-session --drm --codex`; processos compositor/shell também são os instalados em `/usr/bin`. Logs confirmam Codex/painel/dock mapeados. É execução instalada real, sem dependência de binários em /srv/JLinux, mas ainda não é uma sessão autenticada através do novo greeter.

### Verificação adicional antes de encerrar a instalação

A checagem final encontrou a primeira sessão instalada já encerrada; o log antigo não distinguia o status de compositor e shell. Foi preservado localmente e adicionada identificação de código/sinal no supervisor e diagnóstico de fechamento/configuração do shell. Uma sessão de diagnóstico foi iniciada e permaneceu ativa após o terminal inicial encerrar. A causa daquele encerramento isolado ainda não foi comprovada; não registrar como corrigida apenas por adicionar logs.

Também foi corrigido um caso concreto de extensão vazia durante rearranjo de saída: não configurar layers com área de saída nula, e manter último buffer do shell diante de configure sem extensão desenhável. Onze testes locais release e ASan/UBSan passaram novamente. Reempacotamento e teste físico de recuperação estão em andamento.

### Falha de recuperação por VT reproduzida e corrigida

Um teste real com `chvt 2` reproduziu saída normal do compositor: logind pausou o DRM e o wlroots removeu temporariamente HDMI-A-1; a política antiga encerrava o compositor quando output_count chegava a zero e fechava os layers do shell. Essa falha concreta foi identificada; não é prova de que explica todo encerramento anterior sem logs suficientes.

Correção: em sessão DRM, manter compositor vivo sem saídas, desassociar/ocultar layers até haver saída disponível e reassociá-los no retorno. Em backend aninhado/headless, comportamento de encerrar na última saída permanece. Layers novos também podem aguardar saída, sem ser destruídos imediatamente. Teste real VT1 -> VT2 -> VT1 manteve supervisor/compositor/shell vivos e os logs confirmaram novo modeset HDMI-A-1 1600×900@60 no retorno. Não equivale à inspeção visual da tela pelo assistente ou teste manual da combinação de teclas.

Script manual reproduzível em scripts/check-vt-recovery.py, fora da CI. Onze testes release e ASan/UBSan passaram novamente. Nova atualização do pacote em andamento.

### Recuperação com janelas abertas

Repetir a troca de VT com um terminal aberto revelou uma segunda falha: assert de listener pendente em `wlr_output_finish`. A scene_output não era destruída explicitamente no callback de remoção da saída. Adicionada destruição antes de liberar monitor, removendo referências/listeners da cena em tempo. Após essa correção, três ciclos reais VT1 -> VT2 -> VT1 preservaram quatro processos (supervisor, compositor, shell e Foot) e onze testes release passaram. ASan/UBSan também repetidos nesta etapa. Não tratar o primeiro teste sem terminal como validação suficiente de lifecycle com janelas.

## Boot após relato de congelamento — 05/11/2026

Usuário relatou PC travado e o sistema reapareceu em novo boot. Journal atual registra greetd iniciado às 16:40:12, sessão greeter e autenticação PAM do usuário jlinux com abertura de sessão às 16:40:27. Supervisor PID 667, compositor 668 e shell 690 são binários instalados em `/usr/bin`, pacote `0.5.0.r37.g77ecbc1-1`, sob sessão logind autenticada pelo greetd. Foot/Codex em uso. Isso valida primeiro boot e login real; não é resultado simulado.

O teste final de VT da instalação anterior perdeu seu processo/sessão de execução durante o reinício; resultado não recuperado, portanto não declarar aprovação desse teste de pacote. Três ciclos na build local com terminal e onze testes do pacote estão registrados separadamente.

Journal anterior encerra após comandos chvt às 16:38:36, sem registro conclusivo de GPU hang/OOM/kernel panic. Journal novo indica desligamento não limpo e aviso FAT para a partição EFI. Não foi identificado evento suficiente para atribuir causa ao congelamento nem executado reparo de partição. Sessão atual apresenta memória disponível e compositor em espera epoll entre eventos; investigação do incidente permanece aberta. Não repetir testes físicos de VT nesta sessão de trabalho.

## Remoção da marca da barra — fix/remove-panel-brand

Build e teste local shell-headless passaram. Pacote Arch `0.5.0.r39.g766bcaf-1` compilado da branch correta com onze testes aprovados e instalado pelo pacman. Prévia atualizada. Nenhum teste novo ou repetição de ASan para essa remoção de texto/layout. Sessão autenticada pelo greetd preservada; ativação visual ocorre no próximo login. Não confundir versão instalada com processo antigo em execução.

## Aviso de locale ao iniciar o terminal — fix/terminal-locale

Ambiente do Foot na sessão real não continha LANG/LC_*, e shell.log registrava "C is not a UTF-8 locale". /etc/locale.conf contém LANG=pt_BR.UTF-8, disponível em locale -a. Correção no iniciador C carrega LANG somente quando não fornecido pelo ambiente, com leitura sem execução do arquivo e fallback C.UTF-8. Categorias LC_* explícitas não são alteradas.

Doze testes release e ASan/UBSan passaram. Teste unitário cobre locale entre aspas/comentário, preferência herdada, entrada inválida e fallback. Teste de sessão remove variáveis de locale antes de iniciar, verifica LANG nos filhos e ausência do aviso UTF-8 no log do terminal real. Avisos de capacidades Wayland permanecem e não foram suprimidos. Não houve teste de VT ou reinício de sessão.

Pacote `0.5.0.r41.g94ddb52-1` compilado da branch correta com doze testes aprovados e instalado pelo pacman. Novo Foot aberto na sessão existente com LANG=pt_BR.UTF-8; permaneceu ativo e seu log não contém aviso de locale. Os avisos de protocolos permanecem. Não houve reinício/logout/VT. Processos antigos continuam com ambiente anterior até novo login; o iniciador instalado corrige a herança nas sessões futuras.

## Gerenciador de arquivos — feature/file-manager

- FILES-01 implementado: aplicativo C/Wayland separado, toolkit próprio, barra Glass, pastas primeiro, seleção/rolagem, navegação, ocultos e abertura pelo aplicativo associado via GIO sem shell. Primeiro ícone do dock e entrada desktop integrados. Sem operações de escrita de arquivos nesta etapa.
- Quatorze testes locais release e ASan/UBSan passaram. Novos testes de modelo e UI headless cobrem nomes especiais/UTF-8, links, erro de navegação, teclado, clique e argumentos de abertura; teste do shell verifica lançamento pelo dock. Prévia com dados fictícios inspecionada. Sem benchmark ou teste de VT.
- Empacotamento/ativação ainda pendentes neste registro. Novo dock exige próximo login; aplicativo pode abrir na sessão existente. Limites em docs/files.md.

**FILES-01 · instalação:** pacote `0.5.0.r43.gf98f87e-1`, fonte f98f87e, compilado do GitHub com 14 testes aprovados no makepkg e instalado pelo pacman. `/usr/bin/jlinux-files` aberto na sessão autenticada existente e permaneceu ativo após inicialização. Shell/compositor existentes preservados, sem logout, reinício ou troca de VT. Novo dock só no próximo login. Inspeção visual física pelo usuário pendente; CI ainda em execução neste registro. PR #30, base temporária fix/terminal-locale, sem merge.

## Bordas neon discretas — feature/window-neon-rim

Pedido atual substitui pontos focais nas janelas: contorno contínuo de 0,85 px azul/violeta, halo interno discreto de 2,8 px recortado à moldura, sem núcleos brancos. Cabeçalho retangular de 36 px, borda física de 3 px e cantos de 6 px preservados. Dock/barra superior não mudam. Gerenciador recebe fundo opaco em gradiente azul-ardósia mais claro e barra interna com o mesmo acabamento contínuo; não altera conteúdo de aplicativos externos.

Quatorze testes locais release e ASan/UBSan passaram. Teste da moldura passa a conferir continuidade de opacidade e variação cromática em vez dos antigos pontos intensos; controles independentes preservados. Prévias dos componentes inspecionadas junto às referências, sem declarar correspondência exata. Sem timer, animação, blur novo ou benchmark. Ativação das molduras exige novo compositor no próximo login; sessão atual preservada devido à investigação STABILITY-BOOT.

**NEON-01 · instalado:** pacote `0.5.0.r45.g13cadc7-1`, fonte 13cadc7, compilado do GitHub com 14 testes aprovados no makepkg e instalado via pacman. Apenas o gerenciador foi reaberto com o novo fundo e barra interna; processo permaneceu ativo. Compositor/shell da sessão preservados: novas molduras globais aguardam próximo login. Sem teste físico de VT, benchmark ou confirmação visual pelo usuário. PR #31 sem merge; CI pendente neste registro.

## Sidebar do Arquivos — feature/files-sidebar

FILES-02 implementado: barra lateral Pessoal/Downloads/Documentos/Imagens/Vídeos com caminhos XDG, destaque da pasta atual, clique e Alt+1…5. Pastas ausentes não são criadas; erro preserva o diretório anterior. Lista reposicionada e coluna de detalhes oculta em janela estreita, mínimo 540×360.

Quatorze testes locais release e ASan/UBSan passaram. Teste headless ampliado abre os cinco locais com HOME/configuração isolados, nomes personalizados com espaços/acentos, verifica argumentos do aplicativo associado e tentativa de pasta ausente. Prévia com arquivos fictícios inspecionada. Sem benchmark, teste de VT ou confirmação visual física. Empacotamento/ativação pendentes neste registro. Alteração local concorrente em src/shell/main.c preservada fora desta entrega.

**FILES-02 · instalado:** pacote `0.5.0.r47.g0578b80-1`, fonte 0578b80, compilado do GitHub com 14 testes aprovados no makepkg e instalado via pacman. Gerenciador instalado reaberto com sidebar na sessão existente e permaneceu ativo. Esta entrega não reiniciou compositor/shell nem realizou troca de VT. PR #33, base temporária feature/window-neon-rim, sem merge. CI e avaliação visual pelo usuário pendentes neste registro. Alterações concorrentes de shell/empacotamento preservadas fora dos commits desta entrega.

## Dock configurável — feature/configurable-dock

DOCK-01 implementado: lista nativa de aplicativos .desktop via GIO, botão +, fixar/remover com persistência atômica por usuário e scroll no dock/lista. Três ações básicas e geometria preservadas. Atalhos adicionais usam monogramas, não ícones reais. Configuração inválida preservada; limite 32; sem comando de shell montado para lançar aplicativos. Modo auxiliar --dock-panel permite ativação sem encerrar a sessão, com fundo próprio opaco; não confundir com material integrado ou recarga do processo antigo.

Testes locais release e ASan/UBSan, prévia e instalação registrados na conclusão abaixo. Fluxo isolado cobre pin, launch, sinais do filho, persistência, scroll, unpin e configuração inválida em ambos os modos. Sem benchmark, teste de VT ou validação de multimonitor/acessibilidade completa. Limites em docs/dock.md. Base na branch do tema escuro para preservar o visual instalado; alterações concorrentes da árvore principal não incluídas.

**DOCK-01 · concluído/instalado:** 17 testes locais release e ASan/UBSan passaram. Pacote `0.5.0.r54.ge7ec423-1`, fonte e7ec423 da branch feature/configurable-dock, compilado do GitHub com 17 testes aprovados e instalado via pacman, preservando a base do tema escuro. `/usr/bin/jlinux-shell --dock-panel` ativado na sessão existente e permaneceu ativo; ambiente herdou LANG=pt_BR.UTF-8. Nenhum aplicativo foi fixado por conta do usuário. Supervisor/compositor/shell principal não foram encerrados; sem VT, logout ou reinício nesta entrega. Runtime principal permanece no código anterior, com novo dock em processo auxiliar; próximo login usa integração do pacote. Prévia dos componentes inspecionada; confirmação visual física e CI pendentes neste registro. PR #39, base feature/darker-desktop, sem merge.
