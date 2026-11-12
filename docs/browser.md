# Navegador experimental do JLinux

Primeira entrega na branch `feature/web-browser`, dependente da branch `fix/terminal-locale`/PR #28. Aplicativo em C; WPE WebKit 2.54/WPEPlatform processa sites em processos separados. Não altera compositor, shell, login ou navegador padrão. O motor ainda é candidato: nenhuma comparação comprovou que seja o mais rápido ou leve no hardware do usuário.

A branch `feature/browser-tabs` acrescenta somente abas e depende de `feature/web-browser`/PR #32. Cada aba mantém uma WebView e seu histórico/DOM; todas compartilham o display, o toplevel Wayland e a sessão de rede persistente (ver [perfil](browser-profile.md)). Somente a aba ativa é marcada como visível no WPE. Ocultar não é suspender: páginas em segundo plano continuam sujeitas ao comportamento do motor, inclusive áudio e trabalho de fundo.

## Compilar e executar

Instalar `wpewebkit` do Arch e as dependências de desenvolvimento do README. Compilar com `meson setup build-browser --buildtype=release -Dbrowser=true` e `meson compile -C build-browser`. A opção é desligada por padrão e independente de `shell`; o PKGBUILD do desktop continua sem instalar o navegador. A instalação de desenvolvimento inclui um `.desktop` somente quando a opção estiver ligada; não registra associação padrão automaticamente.

`build-browser/jlinux-browser [URL]` exige uma sessão Wayland. Sem URL, abre `about:blank` e o campo de endereço. O aplicativo instancia explicitamente Wayland; nunca escolhe DRM, troca VT ou substitui a sessão. Não existe Python no runtime: scripts Python são apenas ferramentas/testes.

| Ação | Controle |
| --- | --- |
| Abrir endereço e selecionar tudo | Ctrl+L |
| Nova aba vazia | Ctrl+T ou “+” na fileira de abas |
| Próxima / anterior | Ctrl+Tab / Ctrl+Shift+Tab; setas na fileira |
| Selecionar aba | Clique no título na fileira |
| Fechar aba | Ctrl+W ou “×”; fechar a última encerra a janela |
| Abrir página / pesquisar | Enter; domínio sem esquema recebe HTTPS, termos abrem a busca do Google |
| Devolver foco à página, mantendo a barra visível | Esc ou clique fora da barra |
| Selecionar tudo / colar | Ctrl+A / Ctrl+V na barra |
| Apagar | Backspace/Delete na barra |
| Histórico | Alt+← / Alt+→; botões ‹ / › na barra |
| Recarregar | Ctrl+R, F5 ou ↻ |
| Fechar | Ctrl+Q ou fechamento xdg pelo compositor |

A barra provisória é uma subsuperfície de 100 px (36 px para abas), sempre visível acima do site, inclusive após carregar, navegar e pressionar Esc. O viewport do motor recebe a altura da janela menos os 100 px do cabeçalho, sem recortar conteúdo. Quando as abas não cabem, a fileira mostra o grupo que contém a aba ativa; as setas e os atalhos percorrem todas, com retorno da última à primeira. Não há limite artificial de quantidade, mas cada aba consome recursos. Usa Cairo/Pango e material compartilhado em `effects/`, com no máximo dois buffers SHM em voo e redesenho por mudanças. Não há timer de animação ou captura contínua da página. Não implementa blur/refração sobre sites. `JLINUX_GLASS_TINT`/`REFLECTION` afetam o material; BLUR não cria um passe sem backdrop. O conteúdo web permanece sob controle do motor, sem retematizar sites.

Abrir/alternar não recarrega páginas: texto em formulários e histórico da aba permanecem. Fechar uma aba libera sua WebView; a vizinha ocupa o lugar da ativa. Fechar uma aba inativa preserva a seleção atual. Erros de carregamento/política são associados à aba de origem; avisos de uma aba oculta não substituem os avisos da página ativa. Endereço ainda não enviado na barra é descartado ao alternar; não há restauração de abas fechadas nem confirmação implementada de formulários antes de fechar.

O campo inicial permite substituir tudo ou acrescentar/remover texto no fim. Edição com cursor, seleção por trecho, IME, acessibilidade e escalas precisam de implementação/validação. A prévia de componente em `docs/design/` não é uma janela completa, visual definitivo aprovado ou prova de equivalência com as fotos.

## Bloqueio de anúncios — feature/browser-adblock

Ativo por padrão antes da primeira navegação em cada aba. Usa o filtro de conteúdo compilado do próprio WebKit, sem extensão, processo permanente adicional, timer ou varredura JavaScript. A lista de domínios AdAway é embutida comprimida no binário: 4.269 regras após eliminar subdomínios redundantes, incluindo duas regras JLinux adicionais. Créditos, licença CC BY 3.0, snapshot e regeneração em [data/browser/README.md](../data/browser/README.md).

Os domínios da lista são bloqueados em carregamentos de terceiros. O caminho `/pagead/` também bloqueia scripts, imagens e requisições de dados, inclusive no próprio domínio. A ação cosmética nativa oculta `ins.adsbygoogle`, `[data-ad-client]` e iframes com ID começando em `google_ads_iframe`. Não oculta genericamente elementos chamados `ads` e não muda o tema dos sites. É cobertura parcial: anúncios servidos junto ao conteúdo, YouTube/streaming, novos domínios e técnicas antibloqueio podem continuar aparecendo. Não é equivalente ao conjunto de regras e scripts do uBlock Origin.

Ctrl+Shift+B alterna o bloqueio na aba ativa e recarrega a página; formulários em memória dessa página podem perder texto ao recarregar. A escolha dura enquanto essa aba existir, inclusive ao navegar para outro site. Outras abas mantêm seu estado e novas abas começam protegidas. Não há whitelist persistida por site nesta versão. A faixa de ajuda e estado abaixo do endereço foi removida.

A primeira abertura compila a lista em `$XDG_CACHE_HOME/jlinux/browser/filters` (ou o cache padrão do GLib). As seguintes carregam o filtro existente identificado pelo SHA-256 das regras. Formato inválido/incompatível recompila pelo WPE. Se a compilação/cache falhar, há diagnóstico no registro mantendo a navegação. `JLINUX_BROWSER_ADBLOCK=0 jlinux-browser` inicia uma execução inteira sem carregar filtros; para reativar nesse caso, reabrir sem a variável. Não altera DNS, hosts, proxy ou outro navegador; não consulta serviço remoto para atualizar listas. Novos snapshots são revisados e distribuídos com o pacote.

Validação local: 18 testes release e cinco testes C/motor do navegador em build ASan/UBSan (`detect_leaks=0`), além da validação de geração da lista. HTTP local confirma bloqueio antes de baixar, script normal preservado, seletor cosmético, conteúdo normal com classe `ads` visível, filtro desativado, reutilização do cache sem recompilar e navegação quando o cache está inutilizável. O teste Wayland confirma alternância por teclado/mouse, recarga e isolamento entre abas, junto das regressões de layout/vídeo/navegação. Prévia estática do cabeçalho inspecionada; não é captura de um site real.

No fixture, ativar o bloqueio reduziu os bytes da resposta do script publicitário de 524.309 para zero, preservando o script necessário. [Registro de tráfego local](performance/browser-blocker-2026-11-06.json). As durações ali são diagnósticos de uma execução por condição, durante testes paralelos; não demonstram ganho geral de velocidade. A compilação inicial tem custo e é reutilizada depois. Ainda faltam medições comparáveis de CPU, RAM/PSS, energia e carregamento em sites reais; não há base para declarar este navegador o mais leve/rápido. O custo do motor continua separado do custo do bloqueador.

Referências da implementação: [WebKit content blockers](https://webkit.org/blog/3476/content-blockers-first-look/) e [API WPE do cache de filtros](https://people.igalia.com/aperez/Documentation/wpe-webkit/method.UserContentFilterStore.load.html); APIs conferidas nos headers WPE 2.54.2 instalados. LeakSanitizer, GPU/HiDPI e compatibilidade com serviços de anúncios reais permanecem pendentes.

Instalado em 06/11/2026: `jlinux-browser-git 0.1.0.r50.gada56a9-1`, código `ada56a9`, após dez checks do pacote. O teste de bloqueio/cache/falha de cache também passou usando `/usr/bin/jlinux-browser` já instalado. Créditos e licença AdAway instalados em `/usr/share/licenses/jlinux-browser-git/`. Janelas existentes foram preservadas; precisam ser reabertas para carregar os filtros. PR #50 enviado; as duas execuções da CI do commit de código passaram antes desta atualização documental. Nenhum merge realizado.

## Página abaixo do cabeçalho — fix/browser-content-layout

O cabeçalho ocupa a largura da janela em uma subsuperfície em `(0, -148)`, acima da origem da página. A geometria automática xdg inclui as duas superfícies; o motor recebe somente a altura útil abaixo do cabeçalho. O tamanho da view é ajustado nas notificações de largura/altura, antes de WPE emitir `resized` para WebKit, evitando enviar temporariamente o tamanho da janela inteira durante troca de abas ou foco. Não há cópia dos quadros web, mudança de motor ou alteração de CSS dos sites.

O cabeçalho trata seus próprios eventos de ponteiro Wayland e cursor; coordenadas de cliques no site continuam locais à página. A implementação usa `wayland-cursor`, já usado pelo shell e fornecido pelo pacote Wayland existente. Pesquisa Google, sandbox e políticas de navegação permanecem; o [perfil agora é persistente](browser-profile.md).

O teste headless verifica `innerHeight = altura da janela - 148`, ausência de viewport intermediário com altura inteira ao alternar abas, cliques reais em botões fixos no topo e rodapé do site, nova aba pelo cabeçalho maximizado, maximizar/restaurar e redimensionar pela borda. Não equivale a uma captura visual nem a teste físico com GPU/HiDPI. Nenhum compositor paralelo foi iniciado no TTY ativo. Passaram os 16 testes locais release e os quatro testes do navegador com ASan/UBSan (`detect_leaks=0`); LeakSanitizer permanece pendente.

Instalado em 06/11/2026: `jlinux-browser-git 0.1.0.r48.gfdcdfdf-1`, código `fdcdfdf`, após os oito checks do pacote. O teste Wayland de layout/entrada também passou usando `/usr/bin/jlinux-browser` já instalado. Janelas existentes foram preservadas e só adotam o layout novo ao reabrir. Correção enviada no PR #48; uma execução da CI passou e outra seguia em execução no momento do registro, antes desta atualização documental.

## Barra persistente e Google — feature/browser-search-bar

Exibição da barra e foco de edição são estados separados. Ctrl+L ou clique no campo seleciona o endereço atual; Enter navega; Esc cancela a edição e restaura o endereço carregado. Cliques fora da barra chegam à página na primeira tentativa. Mudanças de URI atualizam o campo, exceto durante uma edição em andamento. As abas e botões continuam clicáveis sem Ctrl+L.

Termos como `jlinux` e `navegador leve` abrem `https://www.google.com/search?q=…`, com codificação UTF-8 e percent-encoding dos parâmetros. Domínios, localhost, IPs e URLs continuam abrindo diretamente; esquemas explícitos não permitidos e credenciais continuam recusados. Não há sugestões, chamadas a uma API de busca, chave ou requisições a cada tecla: a consulta é enviada ao Google somente ao confirmar Enter. A URL final continua limitada a 4096 bytes.

A sobreposição existente nesta primeira entrega foi corrigida em `fix/browser-content-layout`, descrita abaixo. A separação entre cabeçalho e viewport não depende de modificar o layout dos sites; a filtragem cosmética de anúncios é uma função separada.

Validação local: suíte release com 16 testes, incluindo busca/codificação/recusa de entradas inseguras, cliques na barra depois da navegação e Esc, primeiro clique no conteúdo, abas e reprodução de vídeo. Os quatro testes do navegador também passaram com ASan/UBSan (`detect_leaks=0`); LeakSanitizer segue pendente. Testes automatizados usam compositor isolado headless/Pixman; não comprovam aparência ou desempenho em GPU física.

Instalado em 06/11/2026: `jlinux-browser-git 0.1.0.r46.g6e7897c-1` (código `6e7897c`), após oito checks do pacote. O executável instalado completou uma busca Google com DOM/JavaScript em modo `--probe`; isso não valida o conteúdo/ranking dos resultados nem representa benchmark. Janelas abertas continuam executando a versão anterior até serem reabertas; não foram encerradas. PR #46 enviado para revisão, com CI em execução no momento do registro.

## Vídeo e encerramentos — feature/browser-media

Vídeo HTML5 foi validado em Wayland headless/Pixman com arquivos locais gerados pelo FFmpeg: MP4 H.264/AAC, WebM VP8/Opus e MediaSource com MP4 fragmentado. O teste exige avanço de `currentTime`, dimensão de vídeo válida e quadros decodificados, não apenas detecção de codecs. Os elementos `<video controls>` dos sites apresentam os controles do motor. Autoplay sem som é permitido; autoplay com som continua sujeito à interação do usuário. Não há implementação de Widevine/DRM de serviços comerciais, fullscreen do desktop ou garantia de compatibilidade com todo site de streaming.

Dependências mínimas do pacote: `gst-plugins-base`, `gst-plugins-good` e `gst-libav`, além do WPE. `gst-plugins-bad` é opcional para formatos adicionais. Não é necessário escrever um decodificador próprio ou usar Python em execução. A pré-verificação nativa confirma `autoaudiosink` e `playbin3`: se faltarem, desativa mídia/WebAudio e mostra aviso em vez de permitir um abort do motor. O teste também executa o navegador com diretórios/registro de plugins vazios e confere que continua navegando sem abortar.

**Causa encontrada nos registros:** os cores de WPEWebProcess de 05/11/2026 às 21:51:52 e 22:00:01 abortaram em `MediaPlayerPrivateGStreamer::createAudioSink()` (`libWPEWebKit-2.0.so.1+0x47f7cda`), durante carregamento de HTMLMediaElement. Os símbolos foram obtidos do debuginfod Arch para o build ID `c28cd278047800f9ef8b8b9cd4e893743db3b315`. A implementação WPE 2.54.2 contém `RELEASE_ASSERT(audioSink)` quando não consegue criar o sink. `gst-plugins-good`, que fornece `autoaudiosink`, não estava instalado, nem os pacotes base/libav. A instalação dos plugins corrige essa dependência ausente; a pré-verificação evita a mesma falha em instalações incompletas. Os dois SIGSEGV antigos do executável de 21:20 pertencem à investigação anterior de limpeza EGL, já corrigida na base; não foram confundidos com esses aborts de mídia.

Fontes: [multimídia no WPE](https://docs.webkit.org/Ports/WebKitGTK%20and%20WPE%20WebKit/Multimedia.html), [createAudioSink na versão 2.54.2](https://github.com/WebKit/WebKit/blob/webkitgtk-2.54.2/Source/WebCore/platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp). Cores e dumps completos permanecem fora do Git.

Também foi testado matar somente o processo de conteúdo com SIGKILL: o navegador permanece aberto, mostra o aviso existente e Ctrl+R recria a página e volta a reproduzir vídeo. Isso não promete recuperar dados que estavam apenas no DOM do processo morto. Sandbox, validação TLS e bloqueio de permissões foram preservados; não há reinício automático em loop.

Nesta máquina foram instalados PipeWire-Pulse, PipeWire-Audio e WirePlumber para fornecer áudio ao navegador. Os sockets/serviços do usuário foram iniciados para esse recurso, sem reiniciar login/compositor. Placas/saídas analógica e HDMI foram detectadas; audição física pelo usuário continua pendente. O JLinux usa runtime Wayland privado: quando não há `PULSE_SERVER`/`PULSE_RUNTIME_PATH` explícitos, o navegador usa o socket padrão `/run/user/UID/pulse/native` somente se existir, for socket e pertencer ao próprio UID. Não altera preferências explícitas, volume ou seleção de saída.

## Instalação persistente e dock

`packaging/arch/browser/PKGBUILD` gera `jlinux-browser-git`, instalado separadamente de `jlinux-desktop-git`. A receita compila/testa a branch `feature/browser-adblock` e empacota apenas `/usr/bin/jlinux-browser` e `org.jlinux.Browser.desktop`. Não executa `meson install` sobre o desktop instalado. O antigo diretório `/tmp/jlinux-web-browser` desapareceu no reboot; a nova worktree de desenvolvimento fica em `~/.local/src/jlinux-browser`, mas a abertura instalada usa `/usr/bin` e não depende dessa worktree.

No dock configurável já instalado, fixar `org.jlinux.Browser.desktop` em `~/.config/jlinux/dock.ini`, preservando os demais IDs, usa a infraestrutura existente e não requer um quarto botão fixo no código. O shell recarrega a configuração ao abrir “+”; role o dock para chegar ao atalho adicional. O catálogo de entradas `.desktop` pode precisar de novo shell/login para reconhecer uma instalação nova. Não reiniciar o compositor para isso. O botão “+” permite remover o atalho posteriormente.

Validação desta entrega: 16 testes locais release e os quatro testes do navegador com ASan/UBSan (`detect_leaks=0`) passaram, incluindo vídeo/MediaSource, ausência de plugins e recuperação de processo web. CI, pacote instalado, teste visual físico, saída de áudio audível e benchmark são evidências separadas. Não foi feito novo benchmark nem teste de VT/DRM.

## Políticas e limites de navegação

Esta versão tem múltiplas abas por janela e [perfil persistente](browser-profile.md): cookies persistentes e armazenamento de sites podem sobreviver entre execuções, respeitando a validade definida pelos sites. Voltar/avançar é histórico em memória de cada aba. Ainda não há favoritos, histórico em disco, extensão, restauração de abas ou suspensão de abas; [downloads estão disponíveis](browser-downloads.md). Links HTTP/HTTPS acionados pelo usuário com `target="_blank"` abrem em outra aba; popups sem gesto do usuário continuam bloqueados.

Endereços e navegações aceitam HTTP/HTTPS e `about:blank`. Esquemas locais/ativos/externos e URLs com credenciais são recusados. Não há interpolação de comandos, ponte JavaScript para funções nativas ou bypass de TLS. A sandbox padrão do WPE permanece ativa; o programa recusa `WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS`. Isso não substitui uma auditoria de segurança ou comprova proteção contra todas as classes de ataque.

Pedidos de câmera/microfone/localização e demais permissões recebidas são negados. Autenticação HTTP é cancelada; formulários de login comuns continuam sendo conteúdo web, ainda sem validação em serviços reais. Downloads são administrados pelo fluxo nativo existente; esquemas locais/externos e janelas sem gesto do usuário continuam bloqueados com aviso. Não há indicador completo de segurança/origem, interface de certificado ou gerenciador de permissões. Não usar como navegador cotidiano nesta etapa.

## Links em nova aba — feature/browser-link-tabs

Um link `target="_blank"` aberto por ação do usuário cria uma aba e carrega diretamente seu destino, sem carregar `about:blank` primeiro. A nova view é relacionada à origem e compartilha a sessão de rede e o processo WebKit conforme a API WPE; isso evita iniciar outro navegador/processo para cada link. Não foi medido ganho de velocidade de carregamento. A validação de URL, TLS, bloqueador, perfil e sandbox permanece aplicada. Popups sem gesto e destinos fora de HTTP/HTTPS ou `about:blank` são recusados. Links comuns continuam na aba atual.

Validação local: 13 testes release passaram; `web-browser-wayland` passou também com ASan/UBSan (`detect_leaks=0`). A regressão usa clique Wayland real contra páginas locais e confirma que o destino abre em aba nova, a página de origem permanece carregada, o link comum navega na aba atual e a aba pode ser fechada. Testes headless/Pixman; não comprovam fluidez em saída física, HiDPI, benchmark de abertura ou ausência de vazamentos de bibliotecas externas.

Instalado em 12/11/2026: `jlinux-browser-git 0.1.0.r68.g6e34f8f-1`, commit `6e34f8f`. O pacote executou 13/13 checks, `pacman -Qkk` confirmou 17 arquivos sem alterações e o teste Wayland passou com `/usr/bin/jlinux-browser` instalado. Versão anterior preservada em `/var/backups/jlinux-browser-before-links-6e34f8f/`. O navegador que já estava aberto não foi encerrado; fechar e abrir novamente para ativar. PR #82 aberto sobre a base temporária PR #77; nenhum merge.

O teste revelou SIGSEGV ao destruir o display Wayland do WPE 2.54.2: o código dessa versão desconecta Wayland antes da limpeza EGL na classe base. O encerramento agora termina EGL após liberar a WebView e antes de liberar o display. É contorno localizado e testado, a remover quando a biblioteca corrigir a ordem. Fontes: [WPEDisplayWayland](https://github.com/WebKit/WebKit/blob/webkitgtk-2.54.2/Source/WebKit/WPEPlatform/wpe/wayland/WPEDisplayWayland.cpp), [WPEDisplay](https://github.com/WebKit/WebKit/blob/webkitgtk-2.54.2/Source/WebKit/WPEPlatform/wpe/WPEDisplay.cpp).

## Testes locais e CI

`meson test -C build-browser --print-errorlogs`: 15 testes release passaram em 05/11/2026. São testes locais, não resultados de CI. Os três testes do navegador verificam endereços, HTTP/DOM/JavaScript/TLS e entrada real no cliente Wayland sobre compositor JLinux headless/Pixman. Os demais são regressões existentes. O fluxo Wayland foi ampliado para criar/alternar/fechar abas por teclado e mouse, preservar texto sem recarregar, manter histórico independente, fechar ativa/inativa/última, percorrer sete abas e encerrar uma janela com várias abas pelo pedido xdg. Também cobre maximização e Ctrl+Q. A CI da base PR #32 passou; verificar a CI da entrega de abas separadamente.

Os testes de integração exigem sockets locais, `bubblewrap` com user namespaces, `openssl` e, para entrada, `wtype`. Quando namespaces não estão disponíveis, os testes do motor/janela retornam 77 (skip), sem desativar a sandbox. A CI compila com `browser=true`; verificar seus resultados separadamente, inclusive possíveis skips de integração em containers.

Build `debug` separado com `-Db_sanitize=address,undefined -Db_lundef=false`: os três testes novos passaram com `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Bibliotecas do sistema não foram recompiladas com sanitizers. LeakSanitizer sem essa opção reportou alocações com stacks em AT-SPI/DBus/WPE e Fontconfig/Pango; a triagem permanece pendente. Não afirmar ausência de vazamentos.

`jlinux-browser --probe https://example.org` passou com TLS normal e JavaScript/DOM prontos. É um único site externo; não comprova vídeo, DRM de mídia, chamadas, aplicativos web complexos ou compatibilidade geral. Nenhum compositor DRM paralelo ou teste de VT foi iniciado; pacote e processos da sessão ativa foram preservados.

## Primeira medição reproduzível

Esta coleta é da primeira versão, anterior às abas. Não estima o consumo atual com múltiplas páginas; não houve novo benchmark nesta entrega funcional.

Hipótese: uma interface nativa pequena pode evitar custo adicional de UI web, mas o custo do motor deve ser medido antes de escolhê-lo. Esta coleta estabelece somente o primeiro ponto de referência, sem testar essa hipótese contra outra interface ou motor.

Comando: `python3 scripts/smoke-web-engine.py build-browser/jlinux-browser --output /tmp/jlinux-web-baseline.json`. Executado sequencialmente, sem sanitizers, após os testes. Três processos novos com sessões efêmeras; biblioteca e cache de arquivos do sistema podem estar quentes. HTTP local mínimo, DOM, JS e um redirecionamento; `LIBGL_ALWAYS_SOFTWARE=1` solicitado. Não há saída física, resolução/frequência ou tempo de apresentação comparável ao desktop. A própria amostragem introduz overhead.

Hardware: Ryzen 7 5700U, RAM utilizável 7,1 GiB, alimentação AC (`ADP0/online=1`). Arch, kernel 7.2.14-arch1-1, WPE 2.54.2-1, Mesa 26.2.6-1, Wayland 1.26.0-1, GLib 2.90.1-1; GCC 16.2.1, build release. Base de código `79d4676` mais a implementação desta branch; SHA-256 do executável está no [registro bruto](performance/browser-headless-2026-11-05.json).

| Execução | Início até DOM/JS prontos | Carga + verificação JS | Pico PSS amostrado da árvore |
| --- | --- | --- | --- |
| 1 | 211,169 ms | 21,345 ms | 234988 KiB |
| 2 (redirect) | 217,701 ms | 32,305 ms | 238442 KiB |
| 3 | 198,942 ms | 22,447 ms | 237163 KiB |

PSS consultado em `/proc` da árvore acessível a cada 25 ms: processos que encerram entre amostras, páginas sem permissão e picos curtos podem ficar de fora. Isso não é memória estável em repouso nem pico exato. O ponto inicial já envolve aproximadamente 229–233 MiB em uma página mínima, portanto ainda não há fundamento para anunciar um navegador “superleve”. O motor representa parte relevante do custo; C não elimina esse consumo.

Próximo comparativo: workloads e perfis equivalentes em WPE, Firefox e Chromium; inicialização com caches quentes/frios separados, 1/5/10 abas quando suportadas, PSS em repouso, CPU/despertares, rolagem, vídeo, compatibilidade e energia. A etapa acelerada será aninhada antes de qualquer ensaio DRM. Não houve medição GPU, bateria, latência ponta a ponta ou equivalência visual com as referências.


## Cabeçalho e abas proporcionais

A faixa mantém todas as abas no mesmo espaço: largura máxima de 180 px, reduzida proporcionalmente quando necessário, com “+” junto à última. Desenho e clique usam o mesmo cálculo, inclusive após redimensionar. Títulos recebem reticências; em abas estreitas, o fechar aparece somente na ativa, e em larguras muito pequenas vira indicador. Ctrl+Tab/Ctrl+Shift+Tab e Ctrl+W continuam disponíveis. Não há limite artificial de abas; muitas abas reduzem a legibilidade e continuam consumindo os recursos dos sites.

Cabeçalho nativo C/Cairo/Pango sobre fundo escuro, aba ativa com reflexo azul-violeta, controles vetoriais e campo arredondado com indicação de foco. A altura permanece 100 px, sem sobrepor o site. Prévias de componente: [uma aba](design/browser-modern-header.png) e [12 abas](design/browser-modern-tabs.png). São geradas pelas mesmas rotinas nativas; não são capturas de páginas nem prova de desempenho em GPU.

A URL mostra ponteiro de texto ao passar o mouse e caret piscante ao digitar. O fim de um endereço longo permanece visível durante edição. O timer de 530 ms existe somente enquanto o campo está em edição e a janela tem foco; sai ao navegar, trocar foco ou encerrar. A edição existente continua acrescentando/apagando no fim do campo.

Ao focar a barra pela primeira vez nesta execução, `webkit_network_session_prefetch_dns` antecipa somente a resolução de `www.google.com`, sem enviar o texto digitado, sem carregar resultados e sem abrir uma página oculta. A API está disponível na biblioteca instalada. Pode reduzir o passo inicial quando o DNS não está em cache; não foi medido um ganho percentual de pesquisa na rede do usuário. O cache normal do WebKit já estava ativado e não foi ampliado. Perfis persistentes, bloqueador, downloads, vídeos e o app WhatsApp são preservados.

Validação: 13 testes locais release passaram com navegador habilitado/shell desabilitado. Incluem perfil/login, downloads, bloqueio, vídeo e WhatsApp. Dois testes direcionados passaram também com ASan/UBSan (`detect_leaks=0`): layout/caret e navegação Wayland, incluindo clique na última de sete abas comprimidas. O teste de layout percorre 1–256 abas em larguras de 240–1920 px, verifica limites, hit testing e diferença de pixels com/sem caret. Inspeção das duas prévias nativas concluída; teste visual físico/HiDPI e benchmark da busca continuam pendentes.


### Instalação do cabeçalho — 07/11/2026

Pacote efetivamente instalado: `jlinux-browser-git 0.1.0.r64.gec7ef34-1`, commit **ec7ef34**, branch `feature/browser-modern-tabs`, PR #73 (base #69). Receita Arch executou os 13 testes novamente; todos passaram. Donos/permissões do pacote conferidos; `pacman -Qkk` confirmou 17 arquivos e zero alterações. Teste Wayland completo também passou usando os binários instalados `/usr/bin/jlinux-compositor` e `/usr/bin/jlinux-browser`, em runtime headless isolado.

Backup dos arquivos anteriores r63/ed57981 e banco do pacote em `/var/backups/jlinux-browser-before-modern-ec7ef34/`; novo pacote em `/var/cache/pacman/pkg/jlinux-browser-git-0.1.0.r64.gec7ef34-1-x86_64.pkg.tar.zst`. Dados do perfil, cookies e armazenamento dos sites não foram modificados por scripts de instalação. O navegador ativo PID 732 manteve o inode/executável anterior, agora `(deleted)` em `/proc`; este PID é evidência deste boot, não instrução para sessões futuras. Ativação ao fechar e reabrir o navegador. Nenhum app ou sessão gráfica foi encerrado.

Prévias copiadas para `~/Imagens/JLinux-browser-header-ec7ef34.png` e `~/Imagens/JLinux-browser-tabs-ec7ef34.png`. Teste unitário de layout também passou com `ASAN_OPTIONS=detect_leaks=1`; teste físico/HiDPI e medição comparativa da busca continuam pendentes.


### Correção da colagem de URLs

A barra passava `NULL` como saída de tamanho para `wpe_clipboard_read_text`. Na biblioteca instalada, copiar o endereço de uma página e colar na barra produzia `g_bytes_unref_to_data: assertion 'size != NULL' failed`, retornando sem o texto. Agora fornece `gsize` válido, negocia formatos UTF-8/text/plain/UTF8_STRING e remove espaços/quebras de linha somente das extremidades do texto colado. Controles internos e esquemas proibidos continuam recusados; não há relaxamento de TLS ou sandbox.

Falhas de abertura/entrada agora aparecem dentro do próprio campo, com contorno de erro, sem reintroduzir ajuda abaixo da barra. Clicar no endereço ou voltar a digitar recupera a edição. O texto digitado e o perfil persistente não são descartados para atualizar a interface.

Regressão Wayland copia uma URL real de textarea da página, com newline final, e cola por Ctrl+L/Ctrl+V/Enter; verifica que a página correta abre e não ocorre o aviso GLib. A fixture aguarda foco/cópia para evitar corrida durante testes paralelos. Houve uma falha de sincronização da fixture na primeira execução paralela; corrigida a espera, a suíte release passou 13/13. Três testes direcionados passaram com ASan/UBSan. Uma URL HTTPS pública também abriu em probe, confirmando que o defeito identificado estava na colagem, não numa falha geral do motor. Não foi fornecida a URL específica do relato para comparar eventual incompatibilidade de site.


Correção instalada em 07/11/2026: `jlinux-browser-git 0.1.0.r66.g38b9ed1-1`, commit **38b9ed1**, branch `fix/browser-direct-url`, PR #77. Check() do pacote 13/13; proprietários/permissões conferidos; `pacman -Qkk`: 17 arquivos e zero alterações. Regressão Wayland com colagem de URL também passou com os binários instalados, inclusive em conjunto com o compositor corrigido r99/161ba52, em runtime headless isolado.

Backup da instalação r64/ec7ef34 e banco local em `/var/backups/jlinux-browser-before-url-fix-38b9ed1/`; pacote novo em `/var/cache/pacman/pkg/jlinux-browser-git-0.1.0.r66.g38b9ed1-1-x86_64.pkg.tar.zst`. Perfil/cookies não foram alterados por scripts de instalação. O navegador existente (PID 759 neste boot) preservou seu inode; ativação ao fechar e reabrir. Não foi encerrado nenhum app ou sessão. O defeito de colagem foi reproduzido com URL controlada; a URL específica do relato continua sem informação para verificar compatibilidade particular do site.
