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

## Barra fina e bateria carregando

Pedido atual: barra de 36 px, sem margem lateral, retangular e com cantos retos, substituindo a barra arredondada de 52 px. Logo/símbolo/texto JLinux e ícone/área de clique de volume removidos da barra. Data/hora central, bateria/porcentagem e botão compacto de ajustes preservados. Controles de volume continuam no centro de controle e no utilitário auxiliar, sem atalho invisível no antigo local da barra.

A bateria mostra um raio azul-claro à esquerda quando o status da mesma fonte de capacity é exatamente Charging. Full, Discharging, Not charging, Unknown e ausência de status não acendem o raio; capacidade ausente/inválida mostra traço. src/shell/battery.c consulta somente sysfs e usa a primeira capacidade válida, mantendo a política de uma bateria da interface anterior. Não agrega múltiplas baterias. Atualização usa o relógio de status existente do shell, sem timer novo.

A prévia futurista-dock-only.png usa 75% e estado Charging ilustrativos, não representa a carga real do computador. Testes de leitura usam diretório temporário e cobrem carga, descarga, cheia, estado ausente e capacidades inválidas. Testes headless de maximização/encaixe foram atualizados para a reserva de 36 px. Alterações compiladas entram no próximo login: o shell ativo não recarrega seu código.

## Bloco Glass de data e hora

Data/hora permanecem centralizadas pela largura medida em Pango, agora com fonte de 14 px (antes 16). O bloco tem 28 px de altura dentro da barra de 36, margem de 4 px acima/abaixo e cantos discretos de 5 px. Reutiliza tonalização escura/translúcida, sombra interna e contorno contínuo azul/rosado das janelas; TINT, SHADOW e REFLECTION permanecem independentes. Não há blur de janelas no relógio nem atualização contínua nova. O bloco continua informativo, sem calendário/menu novo. A prévia mostra o desenho nativo; avaliação na sessão física fica pendente até novo login.

## Volume deslizante no painel de controle

O trilho de volume aceita clique e arraste contínuo entre 0% e 100%, inclusive ao soltar além das pontas. O desenho acompanha a posição imediatamente; o backend wpctl executa um comando por vez e substitui posições pendentes pela mais recente, consultando o estado ao terminar. Falhas e timeout mantêm a indicação de áudio indisponível. O ícone mantém o comando de silenciar. O popup auxiliar conserva seus botões de passo de 5%.

Não perturbe, Economia de energia, Tela noturna e Captura de tela foram removidos do desenho e das ações do painel. Os demais itens subiram 128 unidades, com redução equivalente da altura do painel. Brilho e conectividade mantêm as capacidades anteriores; esta mudança não implementa seus comandos.

Validação: teste unitário cobre rejeição de NaN, limites e substituição de valores pendentes; teste headless com wpctl simulado verifica arraste nos dois sentidos, valor final e limite ao soltar fora do trilho, preservando testes de mute, passos, falha e timeout. Prévia nativa inspecionada; sem avaliação de fluidez na sessão física ou benchmark.

## Dock de aplicativos reais e configurações — 12/11/2026

O catálogo fixo agora contém Brave, Terminal (Foot), Neovim Code, qBittorrent, Arquivos JLinux, Configurações, Imagens, Vídeos, Monitor (btop em Foot) e WhatsApp. Retirados os rótulos/logos Firefox, VS Code e Spotify, cujos executáveis não estavam instalados. Os atalhos de pasta abrem as pastas XDG correspondentes. A engrenagem abre as configurações nativas. Brave já estava instalado; qt6-wayland foi instalado para o qBittorrent.

Os aplicativos gráficos usam seus desktop entries, incluindo o Neovim Code pessoal com a fonte configurada. O launcher respeita a expansão de argumentos do GDesktopAppInfo e não executa uma string via shell. Ícones PNG/SVG são lidos dos diretórios XDG/hicolor/Adwaita na inicialização, renderizados e mantidos em cache. Pastas usam seus ícones de Imagens/Vídeos; a engrenagem nativa mantém seu desenho próprio. Se faltar um aplicativo/ícone em outra instalação, a abertura informa o erro e o fallback visual é a inicial do nome, sem usar a marca de outro aplicativo. A instalação atual teve todos os dez ícones confirmados; programas não são instalados automaticamente pelo clique. O catálogo ainda não tem reordenação/adição livre de aplicativos.

### Configurações na engrenagem

- **Dock:** animação de abrir/fechar, ampliação dos ícones no hover e realce neon selecionado.
- **Aparência:** tonalização, reflexos/contornos e sombras do shell (dock/barra). Não altera preferências gráficas dos aplicativos nem o compositor em execução.
- **Sistema:** som/volume pelo centro de controle, pasta pessoal, monitor e encerrar/reiniciar/desligar com confirmação. Não há chaves falsas para rede, Bluetooth ou energia nessa tela.

As seis preferências são salvas em `~/.config/jlinux/shell.ini`, modo 0600, com substituição consistente. Configuração inválida não aplica valores parcialmente; falha ao salvar mantém o estado anterior e mostra erro. A preferência de animação é respeitada nas aberturas seguintes; JLINUX_REDUCED_MOTION=1 continua impondo movimento reduzido sem sobrescrever a escolha salva. Sombras, tonalização e reflexão continuam independentes; blur mantém seu controle existente.

### Material e seleção

Setores e centro do dock ficaram próximos do preto azulado. O cabeçalho das janelas também foi escurecido pelo material compartilhado (RGB .004/.006/.012, alfa .94). Os setores reutilizam o gradiente de borda ciano/azul/magenta das janelas. O item selecionado ganha halo azul, duas camadas de realce ciano/magenta na borda e ampliação de 12% do ícone. Tudo usa as transições existentes; nenhuma animação permanente ou timer de brilho foi adicionado. Desligar reflexos ou o neon respeita a preferência e não desliga outros efeitos.

Prévia nativa: [dock](design/futurista-dock-only.png), [configurações](design/futurista-settings.png). CLI: `jlinux-shell --preview-settings arquivo.png`. São composições renderizadas, com estado ilustrativo, não captura física nem correspondência exata à referência.

### Validação

Teste headless verifica os nove destinos de abertura com desktop entries/executáveis simulados, argumentos reais das pastas, categorias, preferências após reiniciar o shell e confirmação de energia sem executá-la. Testes unitários verificam decodificação/seleção de ícones e persistência/arquivo inválido/permissões. Probe local confirmou ícones da instalação e versões executáveis de Brave, Neovim, qBittorrent e btop; não abriu contas pessoais. A máscara de sinais do shell passou a ser configurada antes dos carregadores GIO/SVG criarem threads, preservando encerramento, limpeza do runtime e coleta dos comandos de áudio/energia. Checks completos e pacote são registrados na conclusão. Sem benchmark, login em serviços externos ou avaliação física de fluidez.

## Degradê derivado de imagem — 12/11/2026

Pedido: dock menos preto, com degradê, recriado pelo modelo de imagem a partir de um print e reproduzido nativamente. Captura real feita na saída 1600×900 da sessão ativa, com recorte 666×666 do dock. O compositor atual não expõe screencopy ao grim; a captura foi obtida por leitura KMS e conversão VAAPI do FFmpeg, sem modeset, troca de VT ou reinício. O recorte original ficou privado em `~/Imagens/JLinux-dock-antes-degrade.png`, modo 0600, pois os cantos contêm trechos das janelas abertas. Não foi enviado ao repositório; a captura completa temporária foi removida após o recorte.

A geração de imagem editou esse print: faces em grafite/azul-preto, violeta na borda interna e reflexão acetinada. Uma segunda edição corrigiu o ícone do WhatsApp, que o primeiro resultado havia trocado pelo telefone: o ícone instalado usa W. As referências públicas têm o fundo externo limpo, sem os fragmentos das janelas. [Primeira proposta](design/dock-gradient-reference.png), [referência corrigida](design/dock-gradient-reference-v2.png), [implementação nativa](design/dock-gradient-native.png).

`effects/dock.c` implementa o material com gradiente externo→interno em cada setor, gradiente radial no centro e reflexão radial suave independente. A seleção recebe camadas de contorno para suavizar o halo e manter um núcleo violeta/ciano. O dock continua desenhando os ícones reais dos aplicativos; as imagens geradas são referências de design, não a interface interativa. Não foram introduzidos loops de animação permanentes. TINT/REFLECTION e preferências de neon/zoom/movimento reduzido permanecem independentes.

Comparação em 1254×1254 com o WhatsApp selecionado, mesma geometria canônica e fundo neutro no exportador `jlinux-shell --preview-dock arquivo.png`. A comparação visual foi inspecionada; o relatório `design/dock-gradient-comparison.json` registra erro médio de 9,296 níveis RGB em 18 pontos de material fora dos ícones e do setor selecionado. Essa amostragem não mede equivalência da imagem inteira e não demonstra identidade pixel a pixel: rasterização de texto, ícones, textura/reflexo gerado e bordas ainda diferem. Comando reproduzível (ferramenta de análise requer python-pillow):

```sh
scripts/compare-dock-gradient.py docs/design/dock-gradient-reference-v2.png docs/design/dock-gradient-native.png
```

36 testes release passaram, incluindo gradiente não uniforme/mais claro, ausência de preto absoluto no meio e fallback sem tonalização, além de abertura de apps, preferências e transições. O novo material não foi ativado na sessão física durante os testes; instalação/ativação registradas na conclusão. Sem benchmark, novo teste de VT ou promessa de reprodução exata.


O cartão [Bluetooth](bluetooth.md) agora consulta/controla o adaptador real. A parte superior liga/desliga; Dispositivos abre o gerenciador Blueman com busca, emparelhamento e conexão. Os demais controles sem backend continuam indisponíveis; isso não implementa modo avião ou Wi-Fi.

### Spotube no dock

O dock passa a ter onze itens, mantendo os dez anteriores e acrescentando Spotube antes do WhatsApp. Setores, ícones, hover e hit testing usam a mesma divisão em onze partes. O item abre `com.github.KRTirtho.Spotube.desktop`, reutilizando o iniciador local já instalado para o Flatpak e seu ajuste de runtime/portal.

Por pedido do usuário, o Spotube usa o ícone do Spotify, reutilizado do asset local existente. O SVG é instalado como `org.jlinux.Spotube.svg`, evitando conflito com arquivos do aplicativo Spotify. O nome exibido continua Spotube. Prévia nativa inspecionada; reprodução/conta musical não foram alteradas nem testadas nesta mudança.


Spotube instalado no dock em 12/11/2026: `jlinux-desktop-git 0.5.0.r127.g8cc19f9-1`, source 8cc19f9, PR #109. Preserva a instalação efetiva anterior r122/792b37d (barra compacta e Bluetooth). Pacote passou 39/39 testes; dois direcionados ASan/UBSan passaram. Onze ícones instalados validados, incluindo o SVG Spotify do Spotube; fluxo de cliques do dock/configurações também passou usando os binários instalados em runtime headless isolado. Reprodução musical não testada.

`pacman -Qkk`: 28 arquivos, zero alterações. Backup dos arquivos anteriores e banco local em `/var/backups/jlinux-before-spotube-dock-8cc19f9/`; pacote em `/var/cache/pacman/pkg/jlinux-desktop-git-0.5.0.r127.g8cc19f9-1-x86_64.pkg.tar.zst`. Processos existentes 114412/114414/114440 preservaram os inodes; são evidência desta atualização, não PIDs para reutilizar. Nova composição do dock ativa no próximo login; nenhuma sessão/app foi encerrado para atualizar.
