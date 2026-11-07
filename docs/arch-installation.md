# Instalação e login do JLinux no Arch

O desktop continua experimental, mas agora possui instalação rastreável por pacote pacman, sessão Wayland e tela de login própria em C. O login usa o mesmo toolkit/material do shell. `greetd` gerencia autenticação PAM; `cage` isola apenas a tela de login. Após autenticar, o desktop usa **jlinux-compositor**, não Cage. Não há GTK, CSS, webview ou Python no caminho de execução.

## Pacote

A receita de desenvolvimento está em `packaging/arch/PKGBUILD`. Ela acompanha a branch de integração indicada em `source`; a versão identifica contagem e hash do commit. Não é release nem tag. A licença de redistribuição do projeto ainda precisa ser definida; metadado do pacote é `custom`.

Em uma pasta de build, com as dependências do PKGBUILD instaladas:

```sh
cp /srv/JLinux/packaging/arch/PKGBUILD ./PKGBUILD
makepkg --cleanbuild --noconfirm
sudo pacman -U ./jlinux-desktop-git-*.pkg.tar.zst
sudo pacman -S --needed greetd cage
sudo /usr/lib/jlinux/enable-login.sh
```

O configurador salva `/etc/greetd/config.toml` e estados anteriores em `/var/lib/jlinux-login-backup.*`, recusa substituir outro display manager e habilita greetd para o próximo boot. Também habilita `getty@tty2.service` para recuperação. Não encerra a sessão corrente e não reinicia o computador.

Arquivos principais pertencem ao pacote:

- `/usr/bin/jlinux-compositor`, `jlinux-shell`, `jlinux-session` e `jlinux-greeter`;
- `/usr/share/wayland-sessions/jlinux.desktop`;
- `/usr/share/jlinux/greetd.toml`;
- `/usr/lib/jlinux/enable-login.sh`.

## Tela de login

Digite seu usuário do Arch e pressione Enter. A tela mostra a solicitação PAM seguinte; normalmente, a senha. Digite-a e confirme para iniciar o JLinux. Escape cancela a tentativa e retorna ao usuário. Respostas secretas são mascaradas; não há gravação de senhas em arquivo/log. O transporte IPC e buffers temporários são apagados após uso, e core dumps são desativados no greeter. O tratamento de autenticação permanece com PAM/greetd.

O teclado do login usa `br`, configurado em `/etc/greetd/config.toml`. O código suporta composição de acentos via XKB. Nesta primeira versão não há seleção de sessão/usuário em lista, leitor de tela, teclado virtual, colagem ou repetição de tecla. A sessão inicia um terminal Foot; Codex pode ser aberto pelo comando `codex` no terminal.

`jlinux-greeter --demo` abre uma **prévia sem autenticação**, sem conectar ao greetd. `--preview arquivo.png` gera a imagem pelas mesmas rotinas de desenho. Esses modos não entram em uma sessão autenticada e não são usados na configuração de boot.

[Prévia nativa da tela de login](design/native-login-preview.png).

## Recuperação

Na tela de login, Cage permite **Ctrl+Alt+F2**. No desktop DRM do JLinux, o compositor também encaminha os atalhos de troca de VT ao wlroots. Faça login no TTY2 e, para retornar à inicialização por terminal:

```sh
sudo systemctl disable --now greetd.service
sudo systemctl enable getty@tty1.service
sudo systemctl start getty@tty1.service
```

Isso encerra sessões gerenciadas pelo greetd. Para recuperar o arquivo anterior, copie `config.toml` do backup indicado pelo configurador para `/etc/greetd/config.toml`. Consulte `default-target`, `greetd-enabled` e `tty2-enabled` no mesmo backup se precisar repor os estados anteriores. Não remova o pacote enquanto greetd estiver habilitado apontando para seus binários.

## Validação e limites

Testes headless cobrem desenho, ciclo de vida e recusa de iniciar sem greetd. Um servidor simulado cobre prompts secretos/visíveis, mensagens informativas, recusa, cancelamento, início da sessão e mensagens inválidas. Release e ASan/UBSan são verificados separadamente. Esses testes **não comprovam o login PAM real com a senha do usuário**; essa confirmação requer uso da tela real. Boot completo e retomada também precisam de validação física.

Protocolo: documentação primária de [greetd_ipc](https://docs.rs/greetd_ipc/0.10.3/greetd_ipc/). Configuração: manual `greetd(5)` instalado pelo Arch. Não foram alteradas as regras PAM existentes.

## Estado aplicado nesta máquina

Instalado pelo pacman: `jlinux-desktop-git 0.5.0.r37.g77ecbc1-1`, compilado da branch de trabalho, sem release/tag. Binários pertencem ao pacote e são root:root 0755. `greetd.service` é o display manager e `graphical.target` é o alvo padrão. TTY2 habilitado/ativo; backup inicial em `/var/lib/jlinux-login-backup.qsemGPVh`.

Após o reinício do computador relatado pelo usuário em 05/11/2026, o journal confirma a sequência real: greetd iniciou Cage/greeter, PAM abriu a sessão do usuário jlinux e `/usr/bin/jlinux-session --drm` iniciou compositor, shell e Foot instalados. O usuário executou Codex no terminal. Portanto o primeiro boot/login PAM está confirmado por registros reais, não apenas simulação.

O usuário também relatou travamento antes desse reinício. O journal registra desligamento não limpo e não contém evidência suficiente para atribuir a causa do congelamento. A investigação permanece aberta; não repetir ciclos de VT na sessão de trabalho para tentar provocá-lo. O aviso FAT de volume EFI não desmontado corretamente também foi registrado, sem alterações de partição ou reparo automático.

Onze testes locais passaram em release/ASan/UBSan e no build do pacote final. Login UI e protocolo também foram testados com credenciais fictícias, sem invocar PAM nesses testes. Os registros de boot acima constituem a confirmação PAM real, separada dos testes automatizados.

A pausa DRM por troca de VT remove temporariamente a saída neste hardware. O compositor agora preserva layers/sessão e os reassocia quando a saída volta. Foi validada a ida e volta com `chvt`, preservando processos; o teste manual reproduzível é `python scripts/check-vt-recovery.py --session-pid PID --target-vt 2`. Isso não substitui validação do primeiro login PAM.

### Idioma da sessão e do terminal

O iniciador C importa LANG de /etc/locale.conf quando a variável não foi fornecida pelo login. Usa C.UTF-8 se não há valor válido no arquivo; LANG e categorias LC_* já fornecidos pelo usuário são preservados. Não executa o conteúdo desse arquivo. A mudança garante idioma/UTF-8 aos filhos da sessão, sem modificar regras PAM ou habilitar source_profile. Sessões já abertas mantêm seu ambiente até novo login.


### Menu de sessão e energia

O botão no canto direito da barra superior abre **Sair da sessão**, **Reiniciar** e **Desligar**. Cada opção abre uma confirmação com aviso para salvar o trabalho; Cancelar/Fechar não executa a ação. Sair termina somente a sessão identificada por XDG_SESSION_ID; no login greetd, retorna à tela de autenticação. Reiniciar/desligar usam `systemctl --no-ask-password --check-inhibitors=yes`, respeitando autorização e bloqueios do sistema. Não usa sudo, força ou alteração de PAM/PolicyKit. Falhas aparecem no menu e os detalhes seguem para o log do shell. Ainda não há agente gráfico de autenticação: se o sistema exigir autenticação adicional, a ação pode ser recusada.

A sessão já aberta usa um shell supervisionado: encerrá-lo também encerra o compositor. Para disponibilizar somente o novo botão sem fechar aplicativos, `jlinux-shell --power-panel` desenha um botão overlay no canto superior direito e seu menu, sem criar compositor, wallpaper ou reservar área adicional. É um modo de transição para atualizar a sessão existente; no próximo login, o shell normal já inclui o menu e o processo transitório termina junto com o compositor antigo. Deve herdar XDG_SESSION_ID, XDG_RUNTIME_DIR e WAYLAND_DISPLAY da sessão gráfica correta. O log da ativação transitória fica em `/tmp/jlinux-power-live-*/power-menu.log`, numa pasta privada separada para preservar a limpeza do runtime da sessão.

Referências de comportamento: [loginctl](https://www.freedesktop.org/software/systemd/man/262/loginctl.html) e [systemctl](https://www.freedesktop.org/software/systemd/man/latest/systemctl.html).


**Energia · instalação atualizada:** pacote `0.5.0.r50.g023034a-1`, fonte `023034a`, instalado após 15 testes do makepkg aprovados. No boot atual, a sessão greetd 7 preservou supervisor 23096, compositor 23097 e shell principal 23121; cliente transitório `/usr/bin/jlinux-shell --power-panel` PID 41128 foi iniciado com ambiente gráfico da sessão 7 e o compositor registrou `Interface mapeada: jlinux-power-button`. PIDs são evidência deste boot, não identificadores para futuras operações. Log transitório: `/tmp/jlinux-power-live-rfirgnvk/power-menu.log`. No próximo login o shell instalado fornece o menu integrado. Nenhum logout, reboot, desligamento ou teste de VT real foi feito nesta atualização; confirmação física das ações cabe ao usuário quando desejar encerrar a sessão.


### Controle inicial de volume

Clique em **Som**, imediatamente à esquerda do Wi-Fi na barra superior, para consultar o volume da saída padrão, diminuir/aumentar em 5% (limite de 100%) e silenciar/reativar. Clique no estado para consultar novamente; com o menu aberto também há atualização a cada 30 segundos. Durante uma solicitação, cliques adicionais de ajuste não são enfileirados. Fechar recolhe o menu sem alterar o áudio. Sem seleção de saída, microfone, volume por aplicativo ou navegação por teclado nesta primeira versão.

O pacote requer WirePlumber e usa wpctl assíncrono, sem shell, com timeout de três segundos. Falha do comando/ausência de saída padrão aparece como áudio indisponível; o usuário pode tentar novamente. O filho wpctl recebe PIPEWIRE_RUNTIME_DIR=/run/user/UID para acessar o serviço de áudio do usuário, pois o Wayland da sessão usa runtime privado; PIPEWIRE_RUNTIME_DIR explícito é preservado. Não inicia/ativa serviços nem altera a configuração de áudio. Referência: [wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html).

`jlinux-shell --volume-panel` fornece somente o botão e seu menu sobre uma sessão já aberta, sem novo compositor/wallpaper nem reserva adicional de área útil. Esse modo deve herdar o ambiente Wayland da sessão gráfica; termina quando o compositor fecha. No próximo login o shell completo já incorpora o controle. `--preview-volume arquivo.png` desenha uma prévia com 40% fictícios, sem consultar/alterar o áudio real.


Atualização localizada das bordas rosadas: pacote `jlinux-desktop-git 0.5.0.r66.g342e4b4-1`, commit 342e4b4, instalado via pacman. Preserva a remoção dos rótulos do painel de da44b00 e encaixe lateral de 6df67d0. Compilação e 21 testes da base passaram; após incorporar da44b00, shell-headless e window-frame-lifecycle passaram novamente. Backup dos executáveis anteriores em `/var/backups/jlinux-before-pink-eade023/`. O compositor já em execução continua com seu código anterior; ativação da nova decoração depende do próximo login. Sem teste DRM/VT ou reinício de sessão.

### Gerenciador de arquivos padrão

O pacote instala uma associação de sistema em `/etc/xdg/mimeapps.list` para `inode/directory=org.jlinux.Files.desktop`. Preferências pessoais em `~/.config/mimeapps.list` têm precedência. Para selecionar Arquivos JLinux no usuário atual: `gio mime inode/directory org.jlinux.Files.desktop`. Verifique com `gio mime inode/directory` e `xdg-mime query default inode/directory`. A dependência `xdg-utils` fornece `xdg-open` para aplicativos que usam esse comando. Não substitui seletores internos nem implementa o serviço D-Bus `org.freedesktop.FileManager1`.

Associação instalada em 06/11/2026 pelo pacote `jlinux-desktop-git 0.5.0.r69.g850294f-1`; `/etc/xdg/mimeapps.list` pertence ao pacote. O usuário atual também foi configurado. Consulta GIO/XDG e abertura real de Downloads confirmadas. Sem reinício de compositor ou sessão; compilação/empacotamento local validaram o arquivo de configuração. Não houve novos testes de execução do desktop porque os binários não foram alterados por esta configuração.

### Aplicação direta das alterações solicitadas

Por pedido explícito do usuário, alterações futuras devem ser testadas e instaladas no sistema, além do commit/push. Usar o pacote do projeto, preservar as atualizações já instaladas e registrar a versão real. Instalar o pacote não substitui os processos já em execução: aplicar ao vivo quando houver mecanismo seguro; combinar com o usuário qualquer logout/reinício que encerraria aplicativos.

A remoção do dock foi instalada pelo pacman em 06/11/2026: `jlinux-desktop-git 0.5.0.r73.gc81b589-1`, fonte c81b589. A base incorpora as bordas rosadas e a associação de pastas já instaladas, incluindo xdg-utils. Dezenove testes do pacote passaram. `pacman -Qkk` confirmou 20 arquivos e nenhuma alteração; hash do shell instalado coincide com o artefato do pacote. `xdg-mime query default inode/directory` continua retornando org.jlinux.Files.desktop. O iniciador instalado passou em execução headless privada de dois segundos, sem terminal; a prévia instalada do shell não contém dock.

A sessão greetd atual foi preservada: compositor e shell mantiveram os PIDs do boot corrente; `/proc` confirma que o shell continua usando o executável antigo após a atualização. Sem recarga suportada, ativação visual no próximo login. Não reiniciamos a sessão, serviços ou computador; nenhum teste de VT/DRM. O pacote anterior 0.5.0.r69.g850294f-1 está preservado em /var/cache/pacman/pkg para recuperação.

### Prévia Futurista instalada — 06/11/2026

Por autorização explícita do usuário, instalada a interface completa da prévia pelo pacman: `jlinux-desktop-git 0.5.0.r76.g18a1cae-1`, commit 18a1cae, branch `feature/futurist-control-center`. Backup dos cinco executáveis anteriores em `/var/backups/jlinux-before-futurista-18a1cae/`. A dependência de fonte Adwaita já estava instalada. `pacman -Qkk` confirmou 23 arquivos e zero alterações; todos os proprietários/permissões do arquivo do pacote foram conferidos após um aviso interno de fakeroot no empacotamento.

O pacote gera a prévia com o executável instalado por `jlinux-shell --preview-futurista arquivo.png`; cópia em `~/Imagens/JLinux-Futurista-18a1cae.png`. Compositor e shell da sessão existente foram preservados e continuam usando seus executáveis anteriores. A tecla Super e a nova UI ficam disponíveis após sair e entrar novamente. Não houve reinício de serviços, edição PAM, teste VT/DRM nem encerramento dos aplicativos do usuário. Funcionalidades e limites em [futurista.md](futurista.md).

Separação do painel/dock instalada em 06/11/2026: `jlinux-desktop-git 0.5.0.r78.g0e42681-1`, commit 0e42681, branch fix/separate-control-panel. Super abre somente dock; botão de ajustes à direita abre painel, barra sem Wi-Fi/lupa e data/hora centralizadas. Backup anterior em `/var/backups/jlinux-before-control-split-0e42681/`. `pacman -Qkk` confirmou integridade. Sessão atual preservada; ativação ao sair e entrar novamente. Quatro testes headless relevantes e a regressão de alternância com ASan/UBSan passaram; teste visual na sessão física segue pendente.

Dock por pressão de Super instalado em 06/11/2026: `jlinux-desktop-git 0.5.0.r80.g6044cf8-1`, commit 6044cf8. Backup de compositor/shell anteriores em `/var/backups/jlinux-before-super-hold-6044cf8/`. Instalação pelo pacman e conferência `-Qkk` sem arquivos alterados. Pressionar mostra; soltar/retirar o teclado oculta; combinações e seleção por mouse verificadas em ambiente headless, incluindo ASan/UBSan. Sessão existente preservada: novo compositor e shell ativam no próximo login. Teste físico de teclado/DRM não executado.

Seleção angular do dock instalada em 06/11/2026: `jlinux-desktop-git 0.5.0.r83.g67a093d-1`, commit 67a093d. Base 66b04f1 preservada, incluindo animações. Backup de shell/compositor em `/var/backups/jlinux-before-angular-67a093d/`. Pacman confirmou integridade do pacote. Enquanto Super permanece pressionada, movimento/clique fora do círculo selecionam pela direção angular em toda a área abaixo da barra; essa área não repassa cliques às janelas. Três testes locais passaram (normal, movimento reduzido e shell); sem benchmark ou teste físico. A sessão existente foi preservada; ativar no próximo login.
