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
