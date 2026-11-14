# Reutilizar janelas pelo dock

Ao clicar em um aplicativo do dock, o compositor procura uma janela mapeada
com seu app_id. Encontrando, muda para a área de trabalho dessa janela, traz
para frente e restaura se estiver minimizada. Não lança outro processo nesse
caso. Se já houver várias janelas, escolhe a mais recente na ordem de foco.
Não fecha janelas existentes.

A busca usa identificadores dos aplicativos configurados no dock: Brave,
Terminal/foot, Neovim, qBittorrent, Arquivos, Monitor, Spotube e WhatsApp. Inclui
aliases das versões instaladas e a janela Brave/PWA do WhatsApp. Terminal e
Monitor recebem app_ids distintos ao serem abertos pelo dock, preservando a
identidade de cada item. Arquivos, Imagens e Vídeos compartilham a mesma janela;
o caminho de Imagens/Vídeos é usado apenas quando ainda não há Arquivos aberto.

Sem janela correspondente, o lançamento usa o mesmo catálogo/launcher GIO
existente, separado do carregamento de ícones em `apps-launch.c`. Um pedido em
abertura bloqueia novas tentativas daquele app por até dez segundos, ou até a
janela mapear. Assim cliques repetidos não criam várias janelas durante o
startup. A proteção vale para o dock; atalhos e ações internas dos aplicativos
mantêm seu funcionamento.

O shell pede a ação pelo socket datagrama `jlinux-window-control` no runtime
privado da sessão. O compositor aceita somente o índice de um item conhecido,
sem comandos arbitrários. Runtime e socket exigem proprietário e permissões
restritas (0700/0600). Sem o controle do compositor, o shell mostra erro em vez
de abrir sem verificar. Erros de lançamento também voltam ao aviso do shell.
O estado de janelas e lançamentos permanece no compositor ao reiniciar o shell.

Limites: launchers personalizados precisam anunciar app_id reconhecido. Se um
processo encerrar antes de apresentar janela, uma nova tentativa é liberada
ao terminar a espera de dez segundos. Não se trata de bloqueio global de novas
janelas do aplicativo.

## Validação

- `dock-app-targets`: aliases, separação de Terminal/Neovim/Monitor e PWA Brave.
- `dock-single-window`: janelas Wayland simulando cada app, reuso, minimizadas,
  áreas de trabalho, Arquivos compartilhado, cliques rápidos sem duplicação,
  parâmetros de pastas no primeiro lançamento e mensagens inválidas.
- Testes de dock/configurações e reinício do shell verificam reuso do WhatsApp.
- Os três testes de alvos/reuso/configurações passaram com ASan/UBSan e
  detecção de vazamentos. Checks gerais e instalação serão registrados abaixo.

A alteração requer um compositor e shell novos. Atualizar os binários não
encerra a sessão atual; a ativação acontece no próximo login.

Resultado local: os 45 testes release passaram. Quatro cenários (alvos,
reutilização, configurações e reinício do shell) passaram com ASan/UBSan e
detecção de vazamentos. São testes Wayland headless com clientes/launchers de
teste, não inspeção das janelas pessoais abertas do usuário.
