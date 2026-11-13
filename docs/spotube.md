# Música gratuita com Spotube

Instalado no computador em 12/11/2026: Spotube **6.0.0**, Flatpak de sistema
`com.github.KRTirtho.Spotube`, origem Flathub, commit
`105d55f484836bc5cef895769661ef2ce85cf736ffe70bef211861f18f852138`.
O aplicativo ocupa 203,4 MB, além do runtime compartilhado GNOME 51.

Escolhido por oferecer streaming sem assinatura e sem intervalos publicitários,
playlists e suporte nativo a Wayland. Não concede Spotify Premium: metadados
vêm de provedores por plugins e o áudio pode vir do YouTube, pelo NewPipe
incluído. Disponibilidade e correspondência das gravações dependem dessas fontes.
Monophony e Harmony Music originais foram descartados por descontinuação.
Não houve comparação de consumo de memória entre aplicativos.

## Abrir e configurar

Abrir **Spotube** no menu ou executar `spotube` no terminal. Concluir a tela de
boas-vindas e escolher um provedor de metadados na lista de plugins, preferindo
os oficiais. O mecanismo de áudio NewPipe YouTube e letras LRCLib já vêm
incluídos; nenhuma conta pessoal foi conectada durante a instalação.

Instalação reproduzível:

```sh
flatpak install --system flathub com.github.KRTirtho.Spotube
install -Dm755 config/spotube/spotube "$HOME/.local/bin/spotube"
```

Foi copiado o desktop entry exportado pelo Flatpak para
`~/.local/share/applications/com.github.KRTirtho.Spotube.desktop`, alterando
somente `Exec` para o caminho absoluto de `~/.local/bin/spotube %U`.
Esse iniciador preserva PULSE_SERVER existente; caso ausente, usa o socket de
áudio de `/run/user/$(id -u)` quando disponível. Isso permite que o Flatpak
encaminhe o áudio mesmo com XDG_RUNTIME_DIR privado na sessão JLinux.
Não se alterou compositor, sessão, serviço de áudio ou inicialização automática.

Atualizar: `flatpak update com.github.KRTirtho.Spotube`.
Remover: `flatpak uninstall com.github.KRTirtho.Spotube`, removendo depois
somente os dois iniciadores pessoais acima. Não usar `--delete-data` se quiser
preservar configurações e playlists; não remover runtimes compartilhados.

## Validação

- Instalação e desktop entry exportado conferidos via `flatpak info`.
- `sh -n config/spotube/spotube` passou.
- Janela Spotube mapeada em compositor headless/pixman, sem X11.
- Socket PulseAudio disponível dentro do sandbox com PULSE_SERVER explícito.
- Reprodução de música, seleção de provedor, login, qualidade do catálogo,
  consumo de memória e interface na tela física ainda não foram validados.
- Nenhuma sessão pessoal foi encerrada; somente processos do teste foram parados.

Fontes: [projeto oficial](https://github.com/team-spotube/spotube),
[Flathub](https://github.com/flathub/com.github.KRTirtho.Spotube),
[plugins](https://docs.spotube.cc/installing-plugins/),
[Monophony original](https://gitlab.com/zehkira/monophony),
[Harmony Music original](https://github.com/anandnet/Harmony-Music).

## Correção de portal

O portal usa `/run/user/UID`, mas a sessão tem runtime privado. O iniciador
agora usa o runtime canônico também para Flatpak, preservando o socket
Wayland com caminho absoluto. Isso alinha `.flatpak/ID/bwrapinfo.json` com o
diretório consultado pelo portal. Não reinicia o portal nem fecha instâncias
já abertas; reabrir o aplicativo para receber o ajuste.
