# Interface Futurista nativa

A imagem “Central de Controle JLinux Futurista.png” passa a orientar esta tela. Implementação nativa C/Wayland/Cairo/Pango: barra superior, fundo reconstruído, dock radial com dez setores e centro de controle à direita. Não há stack web no shell nem dependência Python na sessão.

## Operação

- Toque em Super sozinha (esquerda ou direita) para mostrar ou esconder somente o dock, ao soltar a tecla. O painel de controle abre e fecha exclusivamente pelo botão de ajustes à direita da barra; alternar para o dock fecha o painel. Super usada com outra tecla, botão ou roda do mouse não abre o dock. Super+Enter permanece abrindo Foot.
- Dock e painel começam ocultos e usam modos exclusivos: desenho e hit test não incluem o componente escondido. Clique fora para fechar. Hover muda o setor em destaque; clique abre o aplicativo. O centro abre o setor selecionado.
- O ícone de Firefox segue a referência; quando Firefox não está instalado, abre `jlinux-browser`. Terminal/Desenvolvimento abrem Foot; Arquivos/Imagens/Vídeos abrem Arquivos JLinux. Code, Spotify e Steam só são executados se instalados; caso contrário há aviso. Não são instalados automaticamente.
- Os controles de volume funcionam: metade esquerda da trilha diminui 5%, direita aumenta 5%, ícone alterna mute. O ícone de som da barra continua abrindo o popover de áudio.
- Encerrar sessão/Reiniciar/Desligar abrem a confirmação existente. Nenhum comando de energia é executado no primeiro clique.
- Relógio, bateria, conectividade Wi-Fi e brilho são consultados no sistema; ausência de dispositivo é indicada. O texto de resolução usa a dimensão lógica da superfície.
- Alteração de Wi-Fi/Bluetooth/modo avião, Não Perturbe, economia de energia, tela noturna, captura de tela, ajuste de brilho e configuração de monitores ainda não estão integrados. Esses cliques mostram indisponibilidade, sem apresentar mudanças de estado falsas.

## Desenho e recursos

A geometria segue a imagem de 1672 × 941, com transformação compartilhada por desenho e hit test, adaptação à área abaixo da barra e fonte Adwaita Sans. Blur amostra apenas o wallpaper e é calculado uma vez em cache reduzido; não desfoca janelas vivas. Tint, reflexão, blur e sombra mantêm controles independentes. Sem animação ou redesenho contínuo: atualiza por mudança de seleção/estado e relógio de 30 segundos. O dock oculto não reserva buffers novos.

O compositor avisa o shell por datagrama local `toggle` em `$XDG_RUNTIME_DIR/jlinux-shell-control`, modo 0600, dentro do runtime privado do usuário. Bind não substitui socket existente. O shell remove o socket que abriu na saída; sua reinicialização é testada. O compositor não conhece desenho ou políticas de lançamento dos aplicativos.

## Evidências e limites

Prévia original histórica com ambos: [futurista-native-preview.png](design/futurista-native-preview.png). Referência: [futurista-reference.png](design/futurista-reference.png). A prévia usa Wi-Fi conectado, bateria 75%, volume 40% e brilho 53% como dados de demonstração; execução real usa consultas ao sistema.

Comparação na mesma resolução: erro absoluto médio RGB normalizado (ImageMagick MAE) caiu de 0,0659633 na primeira prévia para 0,0584804 na quinta. É uma medida de diferença de pixels da tela inteira, incluindo wallpaper e textos; não é uma porcentagem de fidelidade percebida. **Não há correspondência pixel a pixel.** Wallpaper reconstruído, alguns ícones vetoriais, materiais e dados dinâmicos diferem. O usuário não dispõe dos assets separados e autorizou instalar a prévia completa, adiando a reconstrução exata desses assets.

Validação local: 21 testes release; quatro testes de sessão/interface com ASan/UBSan e `detect_leaks=0`, mais o teste do dock repetido após ajuste de layout. Prévia com LeakSanitizer habilitado passou. Esses resultados são locais/headless; não comprovam desempenho ou fidelidade visual em DRM/GPU física. Não houve benchmark de latência/energia nem reinício da sessão ativa.

Gerar a prévia do dock: `jlinux-shell --preview-futurista arquivo.png`; do painel: `jlinux-shell --preview-control-center arquivo.png`. Barra sem Wi-Fi e lupa, com data/hora centralizadas pela largura real do texto e botão de ajustes na direita. [Dock separado](design/futurista-dock-only.png) e [painel separado](design/futurista-control-only.png). A instalação dos executáveis não troca os processos em execução; o novo comportamento de Super exige novo login. Não enviar sinais de recarga ao compositor antigo.

A separação de dock/painel e a barra revista passaram nos testes locais futurista-headless, power-menu-headless, volume-headless e glass-preview; futurista-headless também passou com ASan/UBSan (detect_leaks=0). Sem validação visual da sessão física.
