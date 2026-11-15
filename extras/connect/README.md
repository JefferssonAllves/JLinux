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

A chave aleatória de 256 bits permanece em `~/.local/state/jlinux-connect/pair.key` (0600, diretório 0700). O link usa fragmento, retirado imediatamente do histórico pelo app. A chave não vai para logs HTTP nem localStorage. O login gera outro token em cookie HttpOnly, SameSite=Strict e Secure via HTTPS, com validade de oito horas, ou sem vencimento no servidor quando você escolhe confiar no iPhone até desconectar. O cookie confiável é renovado por um ano de cada vez no uso normal, sem novo link de pareamento. Apenas o hash do token é persistido em `sessions.json` (0600), vinculado à chave de pareamento; reiniciar o serviço/computador preserva o pareamento. Há limite de tentativas de login, validação de Host/Origin, limite de tamanho de mensagens e proteção das rotas WebSocket e do terminal. Sair fecha as conexões daquele dispositivo. Revogar reinicia o gateway e invalida todos os dispositivos.

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

## Conexão automática e ícone no iPhone

O endereço do app é estável. Depois do primeiro pareamento com Confiar neste iPhone, abra sempre o mesmo ícone da Tela de Início. O gateway e o Tailscale iniciam no boot; linger mantém o usuário de serviço disponível sem abrir uma sessão gráfica. Tailscale Serve usa configuração persistente em background. Não é gerado um link novo a cada uso. Revogar ou apagar os dados do Safari/PWA exige pareamento novamente.

O shell público do PWA é salvo pelo service worker para abrir mesmo enquanto o PC está desligado/iniciando. O app aguarda o computador e reconecta automaticamente; nunca reenvia ações de energia, prompts ou uploads sozinho. APIs, conversas, terminal, cookies e arquivos não entram nesse cache. Os dispositivos confiáveis antigos de 30 dias são migrados sem trocar chave/cookie. A chave de máquina do PC e do iPhone também precisa estar sem expiração no painel Tailscale para evitar autenticação periódica da VPN. A VPN do iPhone deve permanecer ligada.

## Controles de mídia

A seção de áudio inclui música, artista, reproduzir/pausar, anterior/próxima e avanço/retrocesso de 10 segundos via MPRIS, o mesmo protocolo da barra nativa. Consulta o barramento gráfico e o barramento de usuário, escolhe inicialmente um player tocando e permite escolher entre vários. Botões respeitam CanControl/CanPlay/CanPause/CanGoNext/CanGoPrevious/CanSeek; não simula teclas multimídia para um aplicativo desconhecido.

## Codex

Execuções concluídas com sucesso ficam como **Finalizado · não lido** até abrir a conversa no celular. O filtro de finalizados e a contagem destacam essas respostas dentro de cada pasta. A leitura é salva no armazenamento local deste navegador, pelo identificador da execução: uma nova conclusão volta a ser não lida. Abrir no PC não altera esse indicador; apagar os dados do navegador remove as marcas de leitura. Tarefas ativas, aguardando resposta, interrompidas ou com erro não são tratadas como conclusão bem-sucedida.

A conversa aberta recebe o texto da resposta em tempo real por WebSocket autenticado, agrupando atualizações em intervalos de 50 ms. Somente mensagens do agente entram nesse fluxo: comandos, arquivos, testes, raciocínio e comentários de progresso são filtrados no servidor. A conexão é refeita ao voltar ao app ou após perda de rede; a consulta periódica recupera o histórico e os estados. Não há garantia de simultaneidade exata: a entrega depende da rede.

A aba usa o app-server real do Codex instalado, via WebSocket no socket Unix local padrão, sem expor a API bruta ou nova porta de rede. A lista agrupa conversas e subagentes pelo caminho completo da pasta associada, com nome, caminho e contagem por grupo; pastas de mesmo nome permanecem separadas. Chats sem caminho ficam em “Sem pasta associada”. Mantém estados de execução/parado/espera/erro e busca. Subagentes gerenciados que não aceitam entrada direta são orientados pelo agente principal, conforme a API do Codex. Permite criar uma conversa em uma pasta existente e enviar prompts, mantendo modelo, política de aprovação e sandbox do agente. Um prompt durante execução usa turn/steer; um agente parado usa turn/start após resume.

Somente prompts humanos e mensagens de resposta são mostrados. Commentary, raciocínio, ferramentas, testes e saídas de comandos não são retornados ao browser. O histórico é paginado, com cache em memória de mensagens públicas e ETag; os estados vêm da API. Perguntas nativas e perguntas assíncronas do TUI podem ser respondidas pelo telefone. Autorizações necessárias aparecem em cartões separados, com a ação solicitada para decisão explícita; nunca são aprovadas automaticamente. Tipos de solicitação não suportados são identificados, não respondidos com uma autorização inventada.

`jlinux-codex.service` inicia o daemon de forma idempotente no boot e usa um cgroup separado: parar/atualizar o companion não encerra os agentes. A instalação não reinicia um daemon Codex existente. Se ele estiver indisponível, o companion solicita início da unidade e tenta reconectar. Requer Codex autenticado no computador. Integração validada com codex-cli 0.169.0 e seus schemas gerados localmente; a API é experimental e pode exigir adaptação em versões futuras. Nenhum prompt real é disparado automaticamente para teste ou inicialização.

## Tela imersiva

Abrir a tela ocupa o visor todo, preservando proporção e a imagem completa. Usa Fullscreen API quando disponível, com fallback CSS para Safari/iPhone e respeito às áreas seguras. O teclado/teclas especiais aparecem em um controle sobre a imagem; Sair da tela cheia devolve a interface sem fechar a conexão. Nítido usa qualidade 9/9 e compressão baixa; Conexão lenta reduz os dados. Captura limitada a até 60 FPS, com GPU habilitada, fallback de inicialização por CPU e H.264/WebCodecs quando negociado pelo NeatVNC e pelo navegador. Não força uma codificação incompatível.

A taxa real, nitidez, uso de GPU e latência dependem do driver, tamanho da tela, browser e rede direta/relay. Não é prometida latência zero ou 60 FPS em qualquer rede. A sessão gráfica precisa estar aberta e o monitor precisa fornecer uma saída capturável.
