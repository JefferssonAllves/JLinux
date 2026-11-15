# Tela cheia e touchpad móvel

Instalado em 15/11/2026 a partir de 4179598, dependente de feature/codex-unread-completion (PR #126).

- 34 testes unitários passaram, incluindo autenticação, rejeição de deltas não finitos/fora dos limites/tipos inválidos/comandos arbitrários e encerramento do cliente de entrada ao desconectar.
- Integração Playwright em compositor headless passou com cliente C real compilado e executado via wlr-virtual-pointer. Dois arrastos iguais em pontos diferentes enviaram deslocamentos iguais; clique, segurar/arrastar e liberação foram conferidos. Controles ocultos e recuperados por dois dedos, Fullscreen API e fallback CSS com rotação retrato/paisagem verificados. Imagem/teclado remoto, mídia, agentes, arquivos e terminal preservados.
- Capturas isoladas inspecionadas: nenhum botão sobre a tela oculta; imagem inteira centralizada em ambas as orientações. Não são capturas do desktop pessoal.
- Cliente C compilou com -Wall -Wextra -Werror; sintaxe JavaScript, shell e git diff --check passaram. Não houve alteração nem recompilação do compositor.
- Instalador oficial aplicado com backup em /var/backups/jlinux-connect-before-touchpad-4179598/app.tar.gz. Sessão autenticada anterior continuou válida. WebSocket do ponteiro conectado ao Wayland real sem enviar movimentos/cliques; arquivo público instalado conferido por HTTPS. Somente gateway reiniciado; daemon Codex preservou socket e agentes. Não havia terminal/comando filho ativo.

No celular: arrastar move relativamente à posição atual, tocar clica, segurar 350 ms antes de arrastar mantém o botão esquerdo. “Ocultar · voltar com 2 dedos” remove controles; dois dedos na imagem os recuperam. Mouse convencional no PC mantém noVNC direto; modo Mover visão continua panorâmico. Faixas preservam proporção e áreas seguras sem cortar a tela.

Não validado em iPhone físico nem medida latência em rede celular. Não prometemos ganho de desempenho medido. Compilação do cliente exige compilador C, pkg-config, wayland-scanner e headers wayland-client; runtime usa libwayland já instalada e inicia apenas durante o controle de tela em dispositivo com ponteiro de toque.
