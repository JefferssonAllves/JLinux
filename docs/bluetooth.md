# Bluetooth no painel de controle

O cartão Bluetooth mostra o estado do primeiro adaptador detectado. Clique na parte superior para ligar/desligar; **Dispositivos ›** abre o Blueman para procurar, emparelhar, conectar e remover dispositivos, incluindo diálogos de PIN/confirmação. Essa gestão detalhada é feita no aplicativo Blueman, não em uma lista própria dentro do painel. Antes de abri-lo, a sessão D-Bus recebe os dois valores Wayland atuais para ativar o agente de emparelhamento no socket correto; a chamada local tem limite de 1 s.

O shell continua em C. Consulta/altera `org.bluez.Adapter1.Powered` por chamadas `busctl` assíncronas, sem shell de comando e sem sudo. Consulta ao abrir o painel e no tick existente de 30 s enquanto ele está aberto. Há só uma operação em andamento; o painel permanece responsivo e o processo é finalizado se ultrapassar 5 s (a chamada D-Bus já possui timeout de 3 s). Bloqueio físico, ausência de adaptador, daemon desligado e falhas de permissão não são mostrados como sucesso. O hardware testado tem um adaptador; escolha entre múltiplos não está implementada.

Dependências: BlueZ e Blueman; `busctl` vem do systemd já exigido pelo desktop. `bluez-utils` foi instalado para diagnóstico. Em uma instalação nova, ativar `sudo systemctl enable --now bluetooth`; se houver bloqueio por software, `sudo rfkill unblock bluetooth`. Não alterar políticas D-Bus nem aceitar automaticamente solicitações de emparelhamento.

## Validação em 12/11/2026

Neste computador o adaptador hci0 existia, mas faltava BlueZ e havia bloqueio por software. Instalados bluez/bluez-utils/blueman; serviço ativado e habilitado, rádios desbloqueados. Consulta e alteração real de Powered para false/true passaram como usuário comum; estado final ligado. Blueman real mapeou janela em compositor headless e sessão D-Bus isolados. Não foi feito emparelhamento com dispositivo físico.

38 testes locais release passaram; três direcionados ASan/UBSan passaram. Novos testes verificam parsing estrito de estado, erro sem atualização otimista, cliques para ligar/desligar, recuperação após falha, abertura do gerenciador e timeout/retentativa sem derrubar o shell. O teste do painel usa busctl e gerenciador falsos para não modificar rádios do host. Prévia nativa do componente inspecionada; status na prévia é ilustrativo. Não é benchmark nem validação de transmissão de áudio/arquivo.

Instalar o pacote não troca o shell em execução. Ativar no próximo login; reiniciar somente o shell encerraria a sessão porque o iniciador monitora sua saída. Não encerrar aplicativos do usuário para aplicar esta alteração.
