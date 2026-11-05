# JLinux Desktop

Ambiente gráfico próprio com compositor Wayland do JLinux em C. O objetivo é personalização de cores, animações, efeitos, funcionalidades e comportamento, com adaptação ao hardware dentro dos limites escolhidos pelo usuário.

O compositor usa wlroots como biblioteca de infraestrutura. O projeto implementará gerenciamento de janelas, composição visual, políticas e shell próprios. Consulte a [arquitetura](docs/architecture.md) e a [análise de hardware](docs/hardware.md).

## Estado atual

O núcleo inicial cria servidor e socket Wayland, inicializa backend/renderizador e saídas e apresenta cena vazia. Encerra com SIGINT/SIGTERM. **Ainda não há suporte a janelas de aplicativos, teclado/ponteiro, animações ou configuração dinâmica.** Não é uma sessão pronta para uso diário.

A [configuração de exemplo](config/jlinux/desktop.example.toml) registra o contrato proposto para preferências e adaptação; ainda não é carregada. O antigo painel GTK permanece opcional, aguardando suporte a layer-shell ou integração futura.

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf wlroots0.20 wayland wayland-protocols mesa
meson setup build --buildtype=release --prefix="$HOME/.local"
meson compile -C build
meson test -C build --print-errorlogs
```

Se `build` já existir, use `meson setup build --reconfigure`. A versão suportada de wlroots é 0.20.x; sua API é instável entre séries.

## Executar o núcleo

Teste sem monitor físico, com renderização em software e encerramento automático:

```sh
scripts/smoke-compositor.sh "$PWD/build/jlinux-compositor"
```

Dentro de uma sessão Wayland, executar aninhado (a janela ficará vazia; encerrar pelo terminal com Ctrl+C):

```sh
WLR_BACKENDS=wayland ./build/jlinux-compositor
```

O backend aninhado com GPU e o backend DRM ainda precisam de validação local. Não substituir sua sessão atual por este protótipo. Para instalar o binário em `~/.local/bin` após configurar o prefixo:

```sh
meson install -C build
```

## Estrutura

- `src/compositor/`: núcleo do compositor próprio.
- `src/panel.c`: protótipo anterior, opcional com `-Dpanel=true`; requer GTK3 e gtk-layer-shell e ainda não funciona dentro do novo compositor.
- `config/jlinux/`: contrato proposto de personalização.
- `docs/`: hardware, arquitetura, evolução e validação.
- `scripts/`: coleta de hardware e teste headless.
- `.github/workflows/`: compilação e teste automatizado em Arch Linux.

## Desenvolvimento

`main` reúne versões estáveis; `develop` integra o próximo ciclo. Funcionalidades, correções e manutenção passam por branches próprias e pull requests. Consulte [CONTRIBUTING.md](CONTRIBUTING.md) antes de alterar o projeto.

O plano inicial está em [TASKS.md](TASKS.md). Perfis de agentes, skills e estado das integrações estão documentados em [docs/agent-tooling.md](docs/agent-tooling.md).
