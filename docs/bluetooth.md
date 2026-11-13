# Bluetooth no painel de controle

O cartão Bluetooth mostra o estado do primeiro adaptador detectado. Clique na parte superior para ligar/desligar; **Dispositivos ›** abre o Blueman para procurar, emparelhar, conectar e remover dispositivos, incluindo diálogos de PIN/confirmação. Essa gestão detalhada é feita no aplicativo Blueman, não em uma lista própria dentro do painel. Antes de abri-lo, a sessão D-Bus recebe os dois valores Wayland atuais para ativar o agente de emparelhamento no socket correto; a chamada local tem limite de 1 s.

O shell continua em C. Consulta/altera `org.bluez.Adapter1.Powered` por chamadas `busctl` assíncronas, sem shell de comando e sem sudo. Consulta ao abrir o painel e no tick existente de 30 s enquanto ele está aberto. Há só uma operação em andamento; o painel permanece responsivo e o processo é finalizado se ultrapassar 5 s (a chamada D-Bus já possui timeout de 3 s). Bloqueio físico, ausência de adaptador, daemon desligado e falhas de permissão não são mostrados como sucesso. O hardware testado tem um adaptador; escolha entre múltiplos não está implementada.

Dependências: BlueZ e Blueman; `busctl` vem do systemd já exigido pelo desktop. `bluez-utils` foi instalado para diagnóstico. Em uma instalação nova, ativar `sudo systemctl enable --now bluetooth`; se houver bloqueio por software, `sudo rfkill unblock bluetooth`. Não alterar políticas D-Bus nem aceitar automaticamente solicitações de emparelhamento.

## Validação em 12/11/2026

Neste computador o adaptador hci0 existia, mas faltava BlueZ e havia bloqueio por software. Instalados bluez/bluez-utils/blueman; serviço ativado e habilitado, rádios desbloqueados. Consulta e alteração real de Powered para false/true passaram como usuário comum; estado final ligado. Blueman real mapeou janela em compositor headless e sessão D-Bus isolados. Não foi feito emparelhamento com dispositivo físico.

38 testes locais release passaram; três direcionados ASan/UBSan passaram. Novos testes verificam parsing estrito de estado, erro sem atualização otimista, cliques para ligar/desligar, recuperação após falha, abertura do gerenciador e timeout/retentativa sem derrubar o shell. O teste do painel usa busctl e gerenciador falsos para não modificar rádios do host. Prévia nativa do componente inspecionada; status na prévia é ilustrativo. Não é benchmark nem validação de transmissão de áudio/arquivo.

Instalar o pacote não troca o shell em execução. Ativar no próximo login; reiniciar somente o shell encerraria a sessão porque o iniciador monitora sua saída. Não encerrar aplicativos do usuário para aplicar esta alteração.


## Instalação concluída

Instalado `jlinux-desktop-git 0.5.0.r119.g0901601-1`, source **0901601**, PR #107. Preservada a barra de mídia da instalação que avançou para d98dcd2/r116 durante o trabalho. Após a integração, 39 testes release passaram; o próprio check() do pacote também passou 39/39. Três testes direcionados ASan/UBSan passaram, incluindo Bluetooth e MPRIS, e o fluxo Bluetooth com falha/timeout/gerenciador passou também usando os binários instalados.

Pacman confirmou 23 arquivos e zero alterações; donos/permissões do arquivo de pacote foram conferidos. Backup da instalação anterior e banco do pacote em `/var/backups/jlinux-before-bluetooth-0901601/`; pacote novo em `/var/cache/pacman/pkg/jlinux-desktop-git-0.5.0.r119.g0901601-1-x86_64.pkg.tar.zst`. Os processos existentes 82452/82453/82477 mantiveram seus inodes; são evidência desta atualização, não PIDs a reutilizar. Sessão e aplicativos preservados. O serviço Bluetooth já está ativo/habilitado e o rádio ligado; a mudança do painel ativa no próximo login. Emparelhamento físico segue pendente.
