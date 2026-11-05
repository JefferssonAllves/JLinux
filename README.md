# JLinux Shell

Base para a interface própria do JLinux: painel em C/GTK e sessão Sway/Wayland. Protótipo inicial com botão de aplicativos, relógio e configuração de janelas/atalhos.

Veja a [análise de hardware](docs/hardware.md) e a [arquitetura](docs/architecture.md) para decisões, limites e próximos passos.

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf gtk3 gtk-layer-shell sway foot fuzzel mesa
meson setup build --buildtype=release --prefix="$HOME/.local"
meson compile -C build
meson install -C build
```

Instala o binário em `~/.local/bin` e a configuração de exemplo em `~/.local/share/jlinux/config`, sem substituir configurações existentes.

## Experimentar

Dentro de Sway, execute `./build/jlinux-panel`. Requer compositor Wayland com `wlr-layer-shell`; X11 e GNOME padrão não atendem esse requisito.

Para experimentar a sessão em Sway aninhado, dentro de uma sessão Wayland compatível:

```sh
PATH="$HOME/.local/bin:$PATH" WLR_BACKENDS=wayland sway -c "$PWD/config/sway/config"
```

Para iniciar pelo TTY, use uma conta comum com acesso de sessão ao dispositivo gráfico:

```sh
PATH="$HOME/.local/bin:$PATH" sway -c "$PWD/config/sway/config"
```

Super+Enter abre Foot; Super+D abre Fuzzel; Super+Shift+Q fecha janela; Super+Shift+E oferece saída. Super+setas muda foco; Super+Shift+Espaço alterna flutuação.

A sessão é experimental e ainda não possui bloqueio de tela ou integração de energia. Consulte a arquitetura antes de adotá-la diariamente.

## Estrutura

- `src/`: painel nativo.
- `config/sway/`: sessão experimental.
- `docs/`: hardware e arquitetura.
- `scripts/`: coleta reproduzível de hardware.
- `.github/workflows/`: compilação com warnings como erros e instalação em staging.

O CI não substitui testes gráficos no hardware real. Esta base ainda não é um desktop completo.
