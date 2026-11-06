# Navegador experimental do JLinux

Primeira entrega na branch `feature/web-browser`, dependente da branch `fix/terminal-locale`/PR #28. Aplicativo em C; WPE WebKit 2.54/WPEPlatform processa sites em processos separados. Não altera compositor, shell, login ou navegador padrão. O motor ainda é candidato: nenhuma comparação comprovou que seja o mais rápido ou leve no hardware do usuário.

A branch `feature/browser-tabs` acrescenta somente abas e depende de `feature/web-browser`/PR #32. Cada aba mantém uma WebView e seu histórico/DOM; todas compartilham o display, o toplevel Wayland e a sessão de rede efêmera. Somente a aba ativa é marcada como visível no WPE. Ocultar não é suspender: páginas em segundo plano continuam sujeitas ao comportamento do motor, inclusive áudio e trabalho de fundo.

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

A barra provisória é uma subsuperfície de 148 px (36 px para abas), sempre visível acima do site, inclusive após carregar, navegar e pressionar Esc. O viewport do motor recebe a altura da janela menos os 148 px do cabeçalho, sem recortar conteúdo. Quando as abas não cabem, a fileira mostra o grupo que contém a aba ativa; as setas e os atalhos percorrem todas, com retorno da última à primeira. Não há limite artificial de quantidade, mas cada aba consome recursos. Usa Cairo/Pango e material compartilhado em `effects/`, com no máximo dois buffers SHM em voo e redesenho por mudanças. Não há timer de animação ou captura contínua da página. Não implementa blur/refração sobre sites. `JLINUX_GLASS_TINT`/`REFLECTION` afetam o material; BLUR não cria um passe sem backdrop. O conteúdo web permanece sob controle do motor, sem retematizar sites.

Abrir/alternar não recarrega páginas: texto em formulários e histórico da aba permanecem. Fechar uma aba libera sua WebView; a vizinha ocupa o lugar da ativa. Fechar uma aba inativa preserva a seleção atual. Erros de carregamento/política são associados à aba de origem; avisos de uma aba oculta não substituem os avisos da página ativa. Endereço ainda não enviado na barra é descartado ao alternar; não há restauração de abas fechadas nem confirmação implementada de formulários antes de fechar.

O campo inicial permite substituir tudo ou acrescentar/remover texto no fim. Edição com cursor, seleção por trecho, IME, acessibilidade e escalas precisam de implementação/validação. A prévia de componente em `docs/design/` não é uma janela completa, visual definitivo aprovado ou prova de equivalência com as fotos.

## Página abaixo do cabeçalho — fix/browser-content-layout

O cabeçalho ocupa a largura da janela em uma subsuperfície em `(0, -148)`, acima da origem da página. A geometria automática xdg inclui as duas superfícies; o motor recebe somente a altura útil abaixo do cabeçalho. O tamanho da view é ajustado nas notificações de largura/altura, antes de WPE emitir `resized` para WebKit, evitando enviar temporariamente o tamanho da janela inteira durante troca de abas ou foco. Não há cópia dos quadros web, mudança de motor ou alteração de CSS dos sites.

O cabeçalho trata seus próprios eventos de ponteiro Wayland e cursor; coordenadas de cliques no site continuam locais à página. A implementação usa `wayland-cursor`, já usado pelo shell e fornecido pelo pacote Wayland existente. O perfil efêmero, pesquisa Google, sandbox e políticas de navegação permanecem os mesmos.

O teste headless verifica `innerHeight = altura da janela - 148`, ausência de viewport intermediário com altura inteira ao alternar abas, cliques reais em botões fixos no topo e rodapé do site, nova aba pelo cabeçalho maximizado, maximizar/restaurar e redimensionar pela borda. Não equivale a uma captura visual nem a teste físico com GPU/HiDPI. Nenhum compositor paralelo foi iniciado no TTY ativo. Passaram os 16 testes locais release e os quatro testes do navegador com ASan/UBSan (`detect_leaks=0`); LeakSanitizer permanece pendente.

## Barra persistente e Google — feature/browser-search-bar

Exibição da barra e foco de edição são estados separados. Ctrl+L ou clique no campo seleciona o endereço atual; Enter navega; Esc cancela a edição e restaura o endereço carregado. Cliques fora da barra chegam à página na primeira tentativa. Mudanças de URI atualizam o campo, exceto durante uma edição em andamento. As abas e botões continuam clicáveis sem Ctrl+L.

Termos como `jlinux` e `navegador leve` abrem `https://www.google.com/search?q=…`, com codificação UTF-8 e percent-encoding dos parâmetros. Domínios, localhost, IPs e URLs continuam abrindo diretamente; esquemas explícitos não permitidos e credenciais continuam recusados. Não há sugestões, chamadas a uma API de busca, chave ou requisições a cada tecla: a consulta é enviada ao Google somente ao confirmar Enter. A URL final continua limitada a 4096 bytes.

A sobreposição existente nesta primeira entrega foi corrigida em `fix/browser-content-layout`, descrita abaixo. Não são injetados CSS ou scripts para modificar o layout dos sites.

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

`packaging/arch/browser/PKGBUILD` gera `jlinux-browser-git`, instalado separadamente de `jlinux-desktop-git`. A receita compila/testa a branch `fix/browser-content-layout` e empacota apenas `/usr/bin/jlinux-browser` e `org.jlinux.Browser.desktop`. Não executa `meson install` sobre o desktop instalado. O antigo diretório `/tmp/jlinux-web-browser` desapareceu no reboot; a nova worktree de desenvolvimento fica em `~/.local/src/jlinux-browser`, mas a abertura instalada usa `/usr/bin` e não depende dessa worktree.

No dock configurável já instalado, fixar `org.jlinux.Browser.desktop` em `~/.config/jlinux/dock.ini`, preservando os demais IDs, usa a infraestrutura existente e não requer um quarto botão fixo no código. O shell recarrega a configuração ao abrir “+”; role o dock para chegar ao atalho adicional. O catálogo de entradas `.desktop` pode precisar de novo shell/login para reconhecer uma instalação nova. Não reiniciar o compositor para isso. O botão “+” permite remover o atalho posteriormente.

Validação desta entrega: 16 testes locais release e os quatro testes do navegador com ASan/UBSan (`detect_leaks=0`) passaram, incluindo vídeo/MediaSource, ausência de plugins e recuperação de processo web. CI, pacote instalado, teste visual físico, saída de áudio audível e benchmark são evidências separadas. Não foi feito novo benchmark nem teste de VT/DRM.

## Políticas e limites de navegação

Esta versão tem múltiplas abas por janela e perfil efêmero: cookies e armazenamento são compartilhados entre as abas da mesma execução e não persistem entre execuções. Voltar/avançar é histórico em memória de cada aba. Ainda não há favoritos, histórico em disco, downloads, extensão, restauração de sessão ou suspensão de abas. Links que pedem novas janelas continuam bloqueados; esta entrega abre abas pelo usuário, sem alterar a política de popups.

Endereços e navegações aceitam HTTP/HTTPS e `about:blank`. Esquemas locais/ativos/externos e URLs com credenciais são recusados. Não há interpolação de comandos, ponte JavaScript para funções nativas ou bypass de TLS. A sandbox padrão do WPE permanece ativa; o programa recusa `WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS`. Isso não substitui uma auditoria de segurança ou comprova proteção contra todas as classes de ataque.

Pedidos de câmera/microfone/localização e demais permissões recebidas são negados. Autenticação HTTP é cancelada; formulários de login comuns continuam sendo conteúdo web, ainda sem validação em serviços reais. Downloads e janelas adicionais são bloqueados com aviso nativo. Não há indicador completo de segurança/origem, interface de certificado ou gerenciador de permissões. Não usar como navegador cotidiano nesta etapa.

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
