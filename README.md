# JLinux Desktop

Ambiente gráfico com compositor Wayland próprio em C, personalização e eficiência como prioridades. A primeira interface já oferece fundo escuro, mensagem central, barra JLinux e relógio, em um processo separado do compositor.

O shell usa GTK3 temporariamente. Toolkit próprio, animações e adaptação ao hardware serão desenvolvidos por etapas. O rótulo `Desktop 01` é estático. **Ainda não há teclado, ponteiro ou janelas interativas de aplicativos.**

## Compilar no Arch Linux

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf wlroots0.20 wayland wayland-protocols mesa gtk3 gtk-layer-shell python
meson setup build-desktop --buildtype=release --prefix="$HOME/.local"
meson compile -C build-desktop
meson test -C build-desktop --print-errorlogs
```

Para um `build-desktop` já configurado nesta versão, usar `meson setup build-desktop --reconfigure`. A opção anterior `panel` foi substituída por `shell`; para migrar builds antigos, usar um diretório novo, como acima. Suportamos wlroots 0.20.x.

## Experimentar a interface

Dentro de uma sessão Wayland, abrir o desktop em uma janela aninhada:

```sh
scripts/run-desktop.py
```

Encerrar com **Ctrl+C no terminal que iniciou o comando**. O script usa runtime privado e encerra apenas os processos que criou; não instala uma sessão de login nem altera a sessão atual. A execução aninhada com GPU ainda precisa de validação local.

Para testar sem janela visível:

```sh
scripts/run-desktop.py --headless --duration 3
```

Para experimentar cores e fontes, editar uma cópia de `config/jlinux/theme.css` e iniciar:

```sh
scripts/run-desktop.py --theme config/jlinux/theme.css
```

O CSS adicional é validado ao iniciar, sem recarga automática. O arquivo `desktop.example.toml` continua sendo proposta de contrato, ainda não carregada.

## Verificação

Os testes cobrem inicialização, saída virtual, socket, mapeamento real das duas superfícies e reinício do shell sem queda do compositor. Headless/Pixman não comprova funcionamento em DRM, qualidade visual na GPU ou eficiência energética. Veja [docs/validation.md](docs/validation.md).

## Organização

- `src/compositor/`: saídas, cena e suporte inicial a layer-shell.
- `src/shell/`: interface visual em processo separado.
- `config/jlinux/`: tema CSS e contrato TOML proposto.
- `protocols/`: protocolo layer-shell com licença e origem preservadas.
- `scripts/`: execução isolada, testes e coleta de hardware.
- `.codex/agents/` e `.agents/skills/`: perfis e skills do projeto.

O plano central está em [TASKS.md](TASKS.md), a arquitetura em [docs/architecture.md](docs/architecture.md) e o hardware em [docs/hardware.md](docs/hardware.md). O fluxo de branches está em [CONTRIBUTING.md](CONTRIBUTING.md); ferramentas e skills em [docs/agent-tooling.md](docs/agent-tooling.md).
