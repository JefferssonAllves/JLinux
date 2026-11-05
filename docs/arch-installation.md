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

O teclado do login usa `br`, configurado em `/etc/greetd/config.toml`. O código suporta composição de acentos via XKB. Nesta primeira versão não há seleção de sessão/usuário em lista, leitor de tela, teclado virtual, colagem ou repetição de tecla. A sessão inicia um terminal Foot; Codex pode ser aberto pelo dock.

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
