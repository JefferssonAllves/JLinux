# JLinux Desktop

Compositor Wayland próprio, shell e iniciador de sessão em C. A primeira base do toolkit JLinux desenha barra superior, texto, ícones e controles de sessão, sem GTK, CSS ou navegador.

Os aplicativos podem ser abertos pelo terminal. O compositor mantém mouse, teclado, foco por clique e atalhos. O terminal inicial é Foot. Esta é a fundação nativa: o visual Glass completo e a reprodução final das fotos ainda não estão implementados.

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf wlroots0.20 wayland wayland-protocols mesa cairo pango fontconfig ttf-dejavu foot wtype python
meson setup build-native --buildtype=release --prefix="$HOME/.local"
meson compile -C build-native
meson test -C build-native --print-errorlogs
```

Requer Meson >=1.6 e wlroots 0.20.x. Python e wtype são usados nos testes, não para executar a sessão. Para reconfigurar o diretório existente: `meson setup build-native --reconfigure`.

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
| Abrir terminal | Super+Enter |
| Mostrar dock circular | Manter Super pressionada; soltar para ocultar |
| Abrir Codex | `codex` no terminal |
| Abrir Arquivos | `jlinux-files` no terminal |
| Alternar janela | Alt+Tab |
| Focar janela | Clique |
| Mover / redimensionar | Super+arraste esquerdo / direito |
| Maximizar / restaurar | Botão ao lado de fechar; Super+↑ / Super+↓ |
| Fechar janela | Botão × ou Super+Q |
| Encerrar sessão | Super+Esc |

Layout físico padrão: `br`; alterar com `--keyboard-layout us`. O botão de energia oferece sair, reiniciar e desligar com confirmação. O controle Som ajusta volume e mute. Os indicadores Wi-Fi/Bluetooth mostram aviso de indisponibilidade ao clicar; os antigos rótulos de menus à esquerda foram removidos. Não há bloqueio/suspensão integrados.

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
- `src/shell/`: cliente Wayland direto com painel.
- `src/toolkit/`: componentes e desenho nativos iniciais.
- `src/session/`: iniciador/supervisor em C.
- `scripts/smoke-*.py`: testes de integração em runtime privado.

Plano: [TASKS.md](TASKS.md). [Arquitetura](docs/architecture.md), [referências do usuário](docs/design/README.md), [validação](docs/validation.md) e [fluxo de branches](CONTRIBUTING.md).

## Instalação no Arch e tela de login

Pacote `jlinux-desktop-git`, login próprio em C/Wayland e inicialização via greetd. O desktop instalado executa de `/usr/bin`; a pasta de desenvolvimento não é necessária para entrar na sessão. O login mantém autenticação PAM do Arch e não usa o modo de demonstração. Consulte [instalação, primeiro login e recuperação por TTY2](docs/arch-installation.md).

[Prévia do login nativo](docs/design/native-login-preview.png). A imagem é uma prévia de desenho; não valida PAM real.

## Gerenciador de arquivos inicial

`jlinux-files [pasta]` abre um cliente nativo C/Wayland com lista de pastas e arquivos. Duplo clique ou Enter abre o item; Backspace sobe uma pasta; F5 atualiza; Ctrl+H mostra ocultos. A barra oferece início, pasta superior e atualização. Arquivos usam a associação do sistema via `gio open`. Veja [os limites e controles](docs/files.md).

## Encaixe lateral de janelas

Janelas abrem livres. Arraste pelo cabeçalho e solte na borda esquerda ou direita para ocupar aquela metade da área útil. Puxar para longe da borda restaura o tamanho anterior. Abrir/fechar outras janelas não reorganiza o desktop. Veja [controles e limites](docs/window-management.md).

O dock lateral foi removido, incluindo lista de aplicativos fixados, configuração e modo de sobreposição. A área útil não tem reserva à esquerda. Arquivos, Terminal e Codex continuam disponíveis pelos comandos acima.

Interface Futurista nativa, evidências e limites: [docs/futurista.md](docs/futurista.md).

## Visão de janelas

**Dois toques rápidos em Super** abrem miniaturas das janelas do espaço atual sobre o desktop desfocado. Clique ou Enter seleciona; Esc cancela. Tab/setas navegam e a roda troca páginas. Veja [controles, recursos e validação](docs/window-overview.md).
