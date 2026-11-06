# JLinux Desktop

Compositor Wayland próprio, shell e iniciador de sessão em C. A primeira base do toolkit JLinux desenha barra superior, dock à esquerda, texto, ícones e botões, sem GTK, CSS ou motor web no shell. O navegador experimental é um aplicativo separado e opcional.

O dock abre Terminal e Codex. O compositor mantém mouse, teclado, foco por clique e atalhos. O terminal inicial é Foot. Esta é a fundação nativa: o visual Glass completo e a reprodução final das fotos ainda não estão implementados.

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf wlroots0.20 wayland wayland-protocols mesa cairo pango fontconfig ttf-dejavu foot wtype python
meson setup build-native --buildtype=release --prefix="$HOME/.local"
meson compile -C build-native
meson test -C build-native --print-errorlogs
```

Requer Meson >=1.6 e wlroots 0.20.x. Python e wtype são usados nos testes, não para executar a sessão. Para reconfigurar o diretório existente: `meson setup build-native --reconfigure`.

## Navegador experimental

```sh
sudo pacman -S --needed wpewebkit
meson setup build-browser --buildtype=release -Dbrowser=true
meson compile -C build-browser
meson test -C build-browser --print-errorlogs
./build-browser/jlinux-browser https://example.org
```

Requer WPE WebKit >=2.54 com WPEPlatform Wayland/headless. Abas com páginas/históricos independentes e sessão efêmera compartilhada. Ctrl+T abre aba, Ctrl+W fecha, Ctrl+Tab/Ctrl+Shift+Tab alternam; a barra nativa aberta por Ctrl+L também oferece títulos, “+” e “×”. Enter navega, Esc retorna à página, Alt+←/→ navega no histórico, Ctrl+R recarrega e Ctrl+Q fecha a janela. Ainda não é navegador para uso diário: downloads e persistência continuam pendentes. Não substitui o navegador padrão nem atualiza o pacote do desktop instalado. [Implementação, testes e limites](docs/browser.md).

## Executar sem Python

No TTY:

```sh
./build-native/jlinux-session --drm --codex
```

Dentro de uma sessão Wayland, para execução aninhada:

```sh
./build-native/jlinux-session --codex
```

O iniciador localiza os binários na própria pasta, cria runtime privado, aguarda prontidão do compositor por pipe e supervisiona processos por eventos. Não instala entrada de login ou serviço. Codex precisa estar instalado/autenticado; sem `--codex`, abre um terminal comum. Usa o diretório atual e inclui `~/.local/bin` no PATH.

| Ação | Controle |
| --- | --- |
| Abrir terminal | Super+Enter ou ícone Terminal no dock |
| Abrir Codex | Ícone de código no dock |
| Mostrar ajuda | Ícone inicial do dock |
| Alternar janela | Alt+Tab |
| Focar janela | Clique |
| Mover / redimensionar | Super+arraste esquerdo / direito |
| Maximizar / restaurar | Botão ao lado de fechar; Super+↑ / Super+↓ |
| Fechar janela | Botão × ou Super+Q |
| Encerrar sessão | Super+Esc |

Layout físico padrão: `br`; alterar com `--keyboard-layout us`. O botão de energia informa como sair; suspensão/reinício/desligamento e os indicadores Wi-Fi/Bluetooth ainda não estão integrados aos serviços. Menus de aplicativos/arquivos/configurações são posições visuais iniciais, com aviso de indisponibilidade ao clicar. Não há bloqueio/suspensão integrados.

## Verificação e preview do desenho nativo

```sh
./build-native/jlinux-session --headless --no-terminal --duration 2
./build-native/jlinux-shell --preview /tmp/jlinux-native.png
```

O preview PNG usa as mesmas rotinas de desenho, sem conectar ao compositor; não é captura de uma sessão real. Cairo desenha os buffers SHM e Pango faz tipografia; o compositor apresenta a cena pela GPU quando disponível. Não afirmamos que o backend atual é o mais rápido: aceleração do toolkit e custos de memória/energia exigem medições.

O cliente mantém no máximo dois buffers em voo por superfície. Redesenha por eventos/alterações e atualiza o relógio a cada 30 segundos. Não há loop de animação contínuo.

O antigo `scripts/run-desktop.py` e o tema CSS foram removidos. TOML continua sendo contrato proposto, sem parser. Animações e efeitos nativos serão implementados separadamente em animation/, effects/ e renderer/.

## Organização

- `src/compositor/`: servidor, saídas, janelas e entrada.
- `src/shell/`: cliente Wayland direto com painel/dock.
- `src/toolkit/`: componentes e desenho nativos iniciais.
- `src/session/`: iniciador/supervisor em C.
- `scripts/smoke-*.py`: testes de integração em runtime privado.

Plano: [TASKS.md](TASKS.md). [Arquitetura](docs/architecture.md), [referências do usuário](docs/design/README.md), [validação](docs/validation.md) e [fluxo de branches](CONTRIBUTING.md).

## Instalação no Arch e tela de login

Pacote `jlinux-desktop-git`, login próprio em C/Wayland e inicialização via greetd. O desktop instalado executa de `/usr/bin`; a pasta de desenvolvimento não é necessária para entrar na sessão. O login mantém autenticação PAM do Arch e não usa o modo de demonstração. Consulte [instalação, primeiro login e recuperação por TTY2](docs/arch-installation.md).

[Prévia do login nativo](docs/design/native-login-preview.png). A imagem é uma prévia de desenho; não valida PAM real.
