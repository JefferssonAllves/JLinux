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
| Abrir página | Enter; domínio sem esquema recebe HTTPS |
| Ocultar barra | Esc |
| Selecionar tudo / colar | Ctrl+A / Ctrl+V na barra |
| Apagar | Backspace/Delete na barra |
| Histórico | Alt+← / Alt+→; botões ‹ / › na barra |
| Recarregar | Ctrl+R, F5 ou ↻ |
| Fechar | Ctrl+Q ou fechamento xdg pelo compositor |

A barra provisória é uma subsuperfície de 148 px (36 px para abas), mostrada temporariamente sobre o site por Ctrl+L e ao abrir/alternar abas; não reduz nem recorta o viewport do motor. Quando as abas não cabem, a fileira mostra o grupo que contém a aba ativa; as setas e os atalhos percorrem todas, com retorno da última à primeira. Não há limite artificial de quantidade, mas cada aba consome recursos. Usa Cairo/Pango e material compartilhado em `effects/`, com no máximo dois buffers SHM em voo e redesenho por mudanças. Não há timer de animação ou captura contínua da página. Não implementa blur/refração sobre sites. `JLINUX_GLASS_TINT`/`REFLECTION` afetam o material; BLUR não cria um passe sem backdrop. O conteúdo web permanece sob controle do motor, sem retematizar sites.

Abrir/alternar não recarrega páginas: texto em formulários e histórico da aba permanecem. Fechar uma aba libera sua WebView; a vizinha ocupa o lugar da ativa. Fechar uma aba inativa preserva a seleção atual. Erros de carregamento/política são associados à aba de origem; avisos de uma aba oculta não abrem a barra sobre a página ativa. Endereço ainda não enviado na barra é descartado ao alternar; não há restauração de abas fechadas nem confirmação implementada de formulários antes de fechar.

O campo inicial permite substituir tudo ou acrescentar/remover texto no fim. Edição com cursor, seleção por trecho, IME, acessibilidade e escalas precisam de implementação/validação. A prévia de componente em `docs/design/` não é uma janela completa, visual definitivo aprovado ou prova de equivalência com as fotos.

## Políticas e limites

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
