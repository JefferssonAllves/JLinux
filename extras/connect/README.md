# JLinux Connect

Companion web para iPhone: volume e mudo do PipeWire em tempo real, visualização/controle da sessão Wayland, terminal interativo, ações do sistema e transferência de arquivos como o usuário do JLinux. A interface acompanha o fundo escuro, vidro discreto e rosa do sistema. No Safari, pode ser adicionado à Tela de Início; não é um aplicativo publicado na App Store.

## Instalar e conectar

Dependências: `python-aiohttp`, `ttyd`, `wayvnc`, `tailscale`, `wireplumber`, além do compositor JLinux com o suporte remoto deste PR. Não precisa de Node, build de frontend, servidor externo ou extensão de navegador. Os módulos noVNC 1.7.0 são baixados com SHA-512 fixado em `fetch-vendor.py` (licença MPL-2.0 incluída; demais arquivos mantêm suas próprias licenças).

```sh
sudo pacman -S --needed python-aiohttp ttyd wayvnc tailscale
./extras/connect/install.sh
jlinux-connect conectar
jlinux-connect parear
```

1. O comando `conectar` solicita o login no Tailscale. Conclua o login no navegador. Se o Tailscale solicitar habilitar HTTPS, siga o link exibido e repita o comando.
2. Instale o Tailscale no iPhone pela App Store e entre na mesma conta. Mantenha a conexão VPN do Tailscale ligada.
3. Saia e entre na sessão gráfica do JLinux para ativar os protocolos de captura e entrada. Não é necessário reiniciar o computador.
4. `parear` exibe um link privado com chave e, quando `qrencode` está disponível, um QR local para a câmera do iPhone. Abra-o no Safari do iPhone. Depois use Compartilhar → Adicionar à Tela de Início. Outra opção é abrir o endereço sem fragmento e colar a chave no formulário.
5. Volume controla a saída de áudio padrão. Tela conecta sob demanda; toque para clicar e use o campo de texto para digitar. Mover visão alterna entre imagem ajustada e panorâmica. Super mantém a tecla pressionada para usar o dock; toque novamente para soltar. Terminal abre um Bash interativo com teclado do iPhone; ao fechar a sessão, os comandos em execução nela podem receber SIGHUP.

O computador precisa estar ligado e conectado à internet. O instalador habilita linger para iniciar o serviço do usuário no boot; a tela exige uma sessão gráfica aberta, e o volume exige áudio ativo. Não acorda um computador desligado. Essa versão não transmite o áudio da área de trabalho ao iPhone; volume ajusta os alto-falantes do computador. A latência depende da rede e do uso de conexão direta ou relay do Tailscale; não é uma solução para streaming de vídeo de alta taxa de quadros.

## Autenticação e limites

O gateway escuta **apenas 127.0.0.1:8765**. Tailscale Serve termina HTTPS e o torna acessível apenas dentro da tailnet; nunca usa Funnel, encaminhamento de portas do roteador ou `0.0.0.0`. O Tailscale continua exigindo conta e autorização do usuário; a instalação não cria uma conta em seu nome.

A chave aleatória de 256 bits permanece em `~/.local/state/jlinux-connect/pair.key` (0600, diretório 0700). O link usa fragmento, retirado imediatamente do histórico pelo app. A chave não vai para logs HTTP nem localStorage. O login gera outro token em cookie HttpOnly, SameSite=Strict e Secure via HTTPS, com validade de oito horas ou 30 dias quando você escolhe confiar no iPhone. Apenas o hash do token é persistido em `sessions.json` (0600), vinculado à chave de pareamento; reiniciar o serviço/computador preserva o pareamento. Há limite de tentativas de login, validação de Host/Origin, limite de tamanho de mensagens e proteção das rotas WebSocket e do terminal. Sair fecha as conexões daquele dispositivo. Revogar reinicia o gateway e invalida todos os dispositivos.

WayVNC e ttyd escutam em sockets Unix privados e iniciam sob demanda. O terminal nunca roda como root; possui os mesmos poderes do seu usuário, incluindo acesso aos seus arquivos e, se autorizado localmente, sudo. A captura já é anunciada pelo compositor para o atalho Print. A entrada remota só é anunciada quando existe `~/.config/jlinux/connect-enabled`, ou com `JLINUX_REMOTE_CONTROL=1` em um teste isolado. Isso permite captura/entrada aos clientes locais que já têm acesso à sessão Wayland; não oferece isolamento por aplicativo nem tela de bloqueio nova.

```sh
jlinux-connect status
jlinux-connect parar       # fecha gateway, conexões e processos de terminal/captura
jlinux-connect iniciar
jlinux-connect revogar     # nova chave e invalidação de todas as sessões
```

Para desativar permanentemente: `systemctl --user disable --now jlinux-connect`; remova `~/.config/jlinux/connect-enabled` e entre novamente na sessão. Para retirar a publicação HTTPS, execute `sudo tailscale serve --https=443 off`. Não apague as configurações de outros serviços do Tailscale.

## Testes

```sh
python3 extras/connect/fetch-vendor.py
python3 -m unittest discover -s extras/connect/tests -v
node --check extras/connect/web/app.js
# Opcional, com Playwright instalado:
JLINUX_PLAYWRIGHT_MODULE=/caminho/node_modules/playwright python3 extras/connect/tests/smoke-desktop.py build/jlinux-compositor
meson test -C build --print-errorlogs
```

Os testes HTTP usam diretório privado, mocks de volume e não controlam a sessão real. O teste de terminal abre somente um shell próprio para verificar a ponte WebSocket e o encerramento. O smoke de captura usa compositor headless separado. Nenhum teste encerra a sessão gráfica ativa. A validação final em Safari/iPhone e a conexão por 4G/5G exigem o aparelho e login no Tailscale.

## Volume ao arrastar

Os eventos de movimento seguem por WebSocket, com envios a cada aproximadamente 40 ms e envio final imediato. O backend aplica um valor por vez e conserva apenas o último pendente; não cria fila de posições antigas. Continua existindo a latência da rede. A rota HTTP permanece disponível como fallback.

## Sistema

A aba Sistema oferece reiniciar, desligar, sair da sessão e atualizar os pacotes oficiais com `pacman -Syu`. Cada ação exige uma confirmação de uso único, vinculada ao dispositivo, ação e prazo de 30 segundos. A atualização requer a autorização de administrador já existente no sistema; o instalador não altera sudoers nem concede NOPASSWD. Se não houver autorização não interativa, o app informa a recusa e o terminal continua disponível para autenticação manual.

As atualizações rodam em unidade do systemd de sistema, separadas do gateway, com resultado e logs consultáveis. Continuam após desconexão, logout ou reinício do app; não são interrompidas pelo comando `jlinux-connect parar`. Um inibidor de desligamento/suspensão protege a transação. O app também rejeita reiniciar/desligar enquanto o trabalho está ativo. Não reinicia automaticamente após atualizar. Pacotes AUR não são atualizados por esse botão.

Ações de energia são agendadas com dois segundos de atraso para o celular receber a confirmação. Sair fecha somente a sessão gráfica identificada e pertencente ao usuário do companion. Não permite escolher outro usuário/sessão pelo HTTP. Computador desligado ou sem rede não pode ser controlado pelo app. A tela de login do sistema continua exigindo uma sessão gráfica normal; o companion não habilita autologin.

## Arquivos

A aba Arquivos navega nas pastas pessoais do usuário. Permite enviar vários arquivos do iPhone sequencialmente, acompanhar progresso/cancelar e baixar arquivos do computador. Cada envio tem limite de 2 GB e usa blocos de 256 KB. Downloads também são transmitidos em blocos. Arquivos existentes nunca são sobrescritos; é necessário renomear o arquivo no iPhone antes de reenviar com o mesmo nome.

Arquivos parciais ficam ocultos e só são publicados após fsync, sem substituição do destino. Cancelamento/falha limpa o arquivo parcial. Acesso usa descritores de diretório e O_NOFOLLOW em cada componente: rejeita caminhos absolutos, `..`, links simbólicos e arquivos especiais. Os testes cobrem travessia, links, FIFO, não sobrescrever, limite de tamanho e espera do worker de disco antes de fechar seu descritor numa requisição cancelada. A listagem mostra até mil itens por pasta; arquivos ocultos podem ser exibidos pelo controle Ocultos.
