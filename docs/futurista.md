# Interface Futurista nativa

A imagem “Central de Controle JLinux Futurista.png” passa a orientar esta tela. Implementação nativa C/Wayland/Cairo/Pango: barra superior, fundo reconstruído, dock radial com dez setores e centro de controle à direita. Não há stack web no shell nem dependência Python na sessão.

## Operação

- Mantenha Super pressionada para mostrar o dock; ao soltar ele desaparece. Super+Enter e demais combinações de teclado cancelam o dock. Seleção com mouse funciona enquanto a tecla está pressionada; abrir um aplicativo mantém o dock até a liberação da tecla. Desconectar o teclado também remove o dock.
- O painel de controle continua abrindo/fechando exclusivamente pelo botão de ajustes da barra; soltar Super não fecha o painel aberto por esse botão.
- Dock e painel começam ocultos e usam modos exclusivos: desenho e hit test não incluem o componente escondido. Clique fora do painel para fechá-lo. Enquanto Super é mantida, o dock recebe movimentos e cliques em toda a área abaixo da barra. A direção angular seleciona o setor mesmo fora do círculo; o centro confirma o último app destacado. Nesse modo os cliques externos são destinados ao dock, não às janelas atrás. Hover muda o setor em destaque; clique abre o aplicativo. O centro abre o setor selecionado.
- O ícone de Firefox segue a referência; quando Firefox não está instalado, abre `jlinux-browser`. Terminal/Desenvolvimento abrem Foot; Arquivos/Imagens/Vídeos abrem Arquivos JLinux. Code, Spotify e Steam só são executados se instalados; caso contrário há aviso. Não são instalados automaticamente.
- Os controles de volume funcionam: metade esquerda da trilha diminui 5%, direita aumenta 5%, ícone alterna mute. O ícone de som da barra continua abrindo o popover de áudio.
- Encerrar sessão/Reiniciar/Desligar abrem a confirmação existente. Nenhum comando de energia é executado no primeiro clique.
- Relógio, bateria, conectividade Wi-Fi e brilho são consultados no sistema; ausência de dispositivo é indicada. O texto de resolução usa a dimensão lógica da superfície.
- Alteração de Wi-Fi/Bluetooth/modo avião, Não Perturbe, economia de energia, tela noturna, captura de tela, ajuste de brilho e configuração de monitores ainda não estão integrados. Esses cliques mostram indisponibilidade, sem apresentar mudanças de estado falsas.

## Desenho e recursos

A geometria segue a imagem de 1672 × 941, com transformação compartilhada por desenho e hit test, adaptação à área abaixo da barra e fonte Adwaita Sans. Blur amostra apenas o wallpaper e é calculado uma vez em cache reduzido; não desfoca janelas vivas. Tint, reflexão, blur e sombra mantêm controles independentes. Sem animação ou redesenho contínuo: atualiza por mudança de seleção/estado e relógio de 30 segundos. O dock oculto não reserva buffers novos.

O compositor avisa o shell por datagramas locais idempotentes `show`/`hide` em `$XDG_RUNTIME_DIR/jlinux-shell-control`, modo 0600, dentro do runtime privado do usuário. Bind não substitui socket existente. O shell remove o socket que abriu na saída; sua reinicialização é testada. O compositor não conhece desenho ou políticas de lançamento dos aplicativos.

## Evidências e limites

Prévia original histórica com ambos: [futurista-native-preview.png](design/futurista-native-preview.png). Referência: [futurista-reference.png](design/futurista-reference.png). A prévia usa Wi-Fi conectado, bateria 75%, volume 40% e brilho 53% como dados de demonstração; execução real usa consultas ao sistema.

Comparação na mesma resolução: erro absoluto médio RGB normalizado (ImageMagick MAE) caiu de 0,0659633 na primeira prévia para 0,0584804 na quinta. É uma medida de diferença de pixels da tela inteira, incluindo wallpaper e textos; não é uma porcentagem de fidelidade percebida. **Não há correspondência pixel a pixel.** Wallpaper reconstruído, alguns ícones vetoriais, materiais e dados dinâmicos diferem. O usuário não dispõe dos assets separados e autorizou instalar a prévia completa, adiando a reconstrução exata desses assets.

Validação local: 21 testes release; quatro testes de sessão/interface com ASan/UBSan e `detect_leaks=0`, mais o teste do dock repetido após ajuste de layout. Prévia com LeakSanitizer habilitado passou. Esses resultados são locais/headless; não comprovam desempenho ou fidelidade visual em DRM/GPU física. Não houve benchmark de latência/energia nem reinício da sessão ativa.

Gerar a prévia do dock: `jlinux-shell --preview-futurista arquivo.png`; do painel: `jlinux-shell --preview-control-center arquivo.png`. Barra sem Wi-Fi e lupa, com data/hora centralizadas pela largura real do texto e botão de ajustes na direita. [Dock separado](design/futurista-dock-only.png) e [painel separado](design/futurista-control-only.png). A instalação dos executáveis não troca os processos em execução; o novo comportamento de Super exige novo login. Não enviar sinais de recarga ao compositor antigo.

A separação de dock/painel e a barra revista passaram nos testes locais futurista-headless, power-menu-headless, volume-headless e glass-preview; futurista-headless também passou com ASan/UBSan (detect_leaks=0). Sem validação visual da sessão física.

Dock por pressão: testes locais futurista-headless, shell-headless, terminal-input-headless e window-side-snap passaram; regressão do dock também verificada com ASan/UBSan. Exercita Super esquerda/direita, soltar, clique segurando, Super+Enter e remoção do teclado.


## Animações iniciais do dock

O dock circular entra com fade e expansão de 18% a 100%, ancorada no centro da tela, em 200 ms; ao soltar Super desaparece em 160 ms. Ao passar entre aplicativos, os setores mudam de destaque e os ícones crescem até 10% em 140 ms, elevam-se até 4 px e recebem halo azul-violeta discreto quando REFLECTION está ativo. As transições usam smoothstep, tempo monotônico e preservam o valor atual ao reverter. Reabrir durante a saída reutiliza a superfície, sem saltar para o começo. O centro mostra o aplicativo selecionado; o painel de controle separado mantém abertura/fechamento imediato.

O shell pede frames pelo callback Wayland somente enquanto há transição, sem timer contínuo. O grupo temporário Cairo fica limitado ao retângulo do dock; o backend ainda rasteriza SHM em CPU e mantém o limite existente de dois buffers por superfície. Nenhuma alegação de FPS, aceleração da animação ou economia de GPU foi medida. Ao fechar, a região de entrada é esvaziada imediatamente; hit testing durante a entrada usa a inversa da mesma matriz gráfica, preservando seleção angular durante a expansão.

`JLINUX_REDUCED_MOTION=1` na inicialização do shell aplica os estados finais imediatamente; `0` ou ausência habilitam as transições. Outros valores são recusados. Essa opção de processo não é persistência de preferências nem recarga ao vivo. Blur/tint/reflection continuam independentes. Instalar o pacote não substitui o shell supervisionado da sessão aberta: animações entram no próximo login.

Seleção angular: removido o limite de distância externa no hit test, mantendo a zona central. Entrada inicial e movimento usam a mesma atualização de seleção; a restauração durante uma animação de fechamento também recupera a região completa abaixo da barra. Testes locais de hover distante + confirmação central, clique distante, animação normal/reduzida e reinício do shell passaram. Não foi feito novo benchmark nem teste físico.

## Entrada pelo centro e realce de seleção

A entrada vertical anterior foi removida. A transformação fica em renderer/dock-transform.c e recebe somente o progresso calculado por animation/transition.c; effects/dock.c desenha o halo sem relógio próprio. O halo respeita REFLECTION e não exige quadros extras: acompanha a transição de hover existente. Movimento reduzido continua aplicando estados finais imediatamente; desativar o halo não desativa escala/fade/hover. A região de rasterização permanece restrita ao retângulo final do dock.

Teste de componente confere pivô invariável no centro, expansão monotônica até escala 1, inversão das coordenadas em todos os estágios e halo nulo quando intensidade é zero. Testes headless exercitam Super, seleção angular e modo reduzido. A imagem de prévia mostra apenas o estado final, não comprova fluidez no hardware. Sem benchmark de CPU/GPU/energia. Instalar não substitui o shell ativo; a nova entrada vale no próximo login.
