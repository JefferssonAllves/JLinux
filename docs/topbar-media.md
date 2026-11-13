# Barra de mídia e relógio neon

A barra nativa recebe uma cápsula de mídia à esquerda e um card central de
data/hora com degradê roxo escuro, contorno violeta, calendário e divisor.
A altura passa de 36 para 56 px; o layer-shell informa a reserva ao compositor.
Os testes de maximização, encaixe, menus e cliques usam essa nova área útil.

A referência foi gerada com imagegen a pedido do usuário:
[imagem](design/topbar-media-reference.png).
A [prévia nativa](design/topbar-media-native.png) usa os mesmos componentes da
barra real. A composição dos cards foi comparada visualmente: tamanhos,
posições, degradês, calendário e divisor foram reproduzidos em Cairo/Pango.
Não é reprodução pixel a pixel: wallpaper instalado foi preservado, fontes
são rasterizadas pelo sistema, relógio/bateria são reais e botões indisponíveis
ficam esmaecidos. A imagem gerada tem 1602×981; a prévia tem 1672×941.

## Mídia

Usa GDBus/MPRIS, sem instalar playerctl, daemon ou subprocessos por clique.
Descobre o primeiro player MPRIS da sessão e acompanha suas propriedades e
seu encerramento por sinais. Mostra título/artista e oferece anterior,
reproduzir/pausar e próxima, respeitando capacidades do player. Sem player,
mostra “Nenhuma mídia / Abra um player” e desabilita os controles.
Ainda não há seletor para vários players simultâneos.

O contexto GLib compartilha o poll de Wayland: sem consultas periódicas ao
player e sem timer dedicado de mídia. As operações D-Bus são assíncronas com
limite de 2 segundos; respostas de instâncias antigas são descartadas.
Metadados longos são limitados preservando UTF-8; título/artista ficam recortados
dentro do card. Efeitos de blur, tonalização, reflexo e sombra permanecem
independentes. O fundo e os ícones pessoais não são substituídos.

[Especificação MPRIS](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html).

## Verificação e aplicação

O teste `media-mpris` cria um player em barramento privado, confere metadados,
os três métodos, capacidades, sinal de pausa, saída do player e cancelamento
na inicialização. Testes Wayland usam headless/pixman; não comprovam desempenho
na GPU ou reprodução em um player pessoal. Instalação e versão serão
registradas após os checks. Não existe recarga segura do shell ativo: encerrar
o shell encerra a sessão supervisionada. A aplicação visual depende de novo
login, sem encerrar aplicativos automaticamente.
