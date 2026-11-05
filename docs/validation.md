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
