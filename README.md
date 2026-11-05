# JLinux Desktop

Compositor Wayland próprio em C, com shell separado, foco por clique, mouse, teclado e terminal Foot. O objetivo é personalização ampla com eficiência medida no hardware real.

A interface inicial oferece fundo, barra superior com botões Terminal/Codex e relógio. GTK3 é a implementação inicial do shell; toolkit próprio, animações e adaptação dinâmica continuam no plano.

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf wlroots0.20 wayland wayland-protocols mesa gtk3 gtk-layer-shell python foot wtype
meson setup build-interactive --buildtype=release --prefix="$HOME/.local"
meson compile -C build-interactive
meson test -C build-interactive --print-errorlogs
```

Para reconfigurar o mesmo diretório, usar `meson setup build-interactive --reconfigure`. Requer Meson >=1.6 e wlroots 0.20.x. `wtype` participa dos testes; Foot fornece o terminal inicial.

## Executar

Dentro de uma sessão Wayland, abrir aninhado com terminal:

```sh
scripts/run-desktop.py
```

A partir do TTY, abrir no monitor com Codex dentro do primeiro terminal:

```sh
scripts/run-desktop.py --drm --codex
```

`codex` precisa estar instalado e autenticado para uso. Sem `--codex`, o terminal abre um shell normal: digite `codex` nele. O script inclui `~/.local/bin` no PATH e abre o terminal na pasta do projeto.

| Ação | Controle |
| --- | --- |
| Abrir terminal | Super+Enter ou botão Terminal |
| Abrir Codex em outro terminal | Botão Codex |
| Alternar janela | Alt+Tab |
| Focar janela | Clique |
| Mover janela | Super+arrastar com botão esquerdo |
| Redimensionar pelo canto inferior direito | Super+arrastar com botão direito |
| Fechar janela focada | Super+Q |
| Encerrar e retornar ao terminal anterior | Super+Esc |

Super é normalmente a tecla Windows. Layout de teclado padrão: brasileiro (`br`); use `--keyboard-layout us` para americano. O layout é aplicado a dispositivos físicos; ferramentas de entrada virtual fornecem o próprio keymap.

O launcher não altera a sessão de login, usa runtime privado e encerra os grupos de processos que criou. Depois da inicialização, aguarda saída dos processos por pidfd, sem polling periódico. A sessão ainda não tem bloqueio ou suspensão integrados.

## Testes e tema

```sh
scripts/run-desktop.py --headless --duration 3
scripts/run-desktop.py --theme config/jlinux/theme.css
```

`--headless` não exibe uma janela. `--duration` é opcional. O CSS adicional é carregado na inicialização; TOML ainda é proposta, sem parser.

O teste de entrada abre dois terminais reais, digita comandos, clica para mudar foco, fecha a janela focada e verifica recuperação de foco. Também verifica saída por Super+Esc. Entrada virtual só é publicada com `JLINUX_TEST_INPUT=1`, usada no runtime privado do teste e desativada na execução normal.

Ainda pendentes: gerenciamento completo de monitores, maximização/tela cheia, arraste solicitado pelas decorações dos clientes, IME, portais, gestos, efeitos e bloqueio. Popups xdg têm suporte inicial, sem posicionamento avançado. Headless não comprova experiência visual ou desempenho da Radeon.

## Organização

- `src/compositor/main.c`: servidor, saídas e layers.
- `src/compositor/interaction.c`: janelas, seat, dispositivos, foco e atalhos.
- `src/shell/`: interface em processo separado.
- `config/jlinux/`: tema CSS e contrato TOML proposto.
- `protocols/` e `tests/`: protocolos com licença preservada e cliente de entrada para teste.
- `scripts/`: execução isolada, testes e coleta de hardware.

Plano central: [TASKS.md](TASKS.md). [Arquitetura](docs/architecture.md), [hardware](docs/hardware.md), [validação](docs/validation.md), [fluxo de branches](CONTRIBUTING.md) e [agentes/skills](docs/agent-tooling.md).
