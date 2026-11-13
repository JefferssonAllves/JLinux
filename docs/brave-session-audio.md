# Áudio do Brave, incluindo modo anônimo

A sessão usa XDG_RUNTIME_DIR privado (`/tmp/jlinux-session-*`), enquanto o áudio
PipeWire/PulseAudio do usuário reside em `/run/user/UID`. Sem configuração
explícita, libpulse procurava `pulse/native` no runtime privado e falhava com
Connection refused. O perfil Brave não tinha bloqueio de som salvo.

O iniciador C agora exporta PULSE_SERVER e PIPEWIRE_RUNTIME_DIR apontando para
o runtime de áudio canônico do próprio usuário, quando esse diretório existe
e pertence a ele. Valores personalizados já definidos são preservados. Não
altera cookies, conteúdo anônimo, permissões dos sites nem configurações de
sandbox do navegador. Não depende de o servidor de áudio já ter criado seu
socket no instante do login.

Na sessão corrente, foi criado somente o link `pulse/native` no runtime
privado para o socket existente do usuário. Isso corrige novas conexões do
Brave já aberto sem encerrá-lo. O link desaparece junto com o runtime; nos
próximos logins a correção persistente é o ambiente exportado pelo iniciador.

Verificações:
- Antes do link, pactl no ambiente da sessão retornava Connection refused;
  depois, conectou ao PulseAudio sobre PipeWire e identificou a saída padrão.
- Brave em Wayland headless, perfil descartável e `--incognito` abriu fluxo de
  áudio (sink-input 86) com `mute=false` e `corked=false`. O teste usou uma página
  local com WebAudio e autoplay autorizado por flag, sem acessar sites/contas.
- Os dois testes do iniciador passaram: caminhos padrão herdados pelos filhos
  e preservação de servidores de áudio personalizados.
- Não houve audição física nem teste no site/guia pessoal do usuário. Recarregar
  a guia e iniciar a reprodução novamente permite repetir a conexão.

Instalação do iniciador atualizado será registrada após os checks do pacote.

Instalado `jlinux-desktop-git 0.5.0.r132.g2d49501-1` via pacman. Os 43 testes
do pacote passaram. A instalação preserva a versão anterior `267b274` e
atualiza apenas o comportamento de ambiente do iniciador. A ponte da sessão
atual continua conectando ao servidor de áudio; não houve reinício de Brave,
sessão ou serviços. PR #112. O ambiente persistente será herdado no próximo
login; a correção ao vivo permite reconexão agora.

## Complemento para processos sem ambiente de runtime

O Brave aberto continuou sem criar streams. Também se reproduziu Connection
refused com `env -u XDG_RUNTIME_DIR -u PULSE_SERVER pactl info`: libpulse usava
um runtime de fallback antigo em `~/.config/pulse/<machine-id>-runtime`.
Foi criado `~/.config/pulse/client.conf` (antes inexistente), modo 0600,
com `default-server = unix:/run/user/1001/pulse/native`. Após isso, a consulta
sem essas duas variáveis conectou ao servidor correto. Não se alterou cookie,
volumes, saída escolhida, perfil do Brave nem abas. A configuração pode ser
revertida removendo somente essa linha/arquivo criado nesta correção.
A reprodução no navegador pessoal ainda aguarda confirmação do usuário.
