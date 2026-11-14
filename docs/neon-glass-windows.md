# Minimização e vidro neon nativo

Implementação em `feature/neon-glass-windows`, sobre a instalação `7df42e2`.
Preserva o dock com Spotube/WhatsApp e a atualização que removeu o terminal
automático no login.

## Janelas

O cabeçalho oferece minimizar (traço), maximizar/restaurar e fechar.
`Super+M` minimiza a janela focada. `Alt+Tab` ou `Super+Tab` e Enter restauram
janelas minimizadas; a visão de janelas inclui suas miniaturas. A janela não
é encerrada e mantém geometria e processo. O pedido xdg `set_minimized` também
é atendido. Não se envia o estado `suspended`, pois o compositor anuncia
xdg-shell v3 e programas de mídia podem continuar tocando minimizados.

Entrada, minimização e restauração usam transição de opacidade. O foco dá
um brilho breve na borda, usando o motor de transições existente. O modo de
movimento reduzido elimina essas transições. Cabeçalhos ficam escuros e
translúcidos, com contorno azul/violeta e halo mais luminoso.

## Material e desfoque real

Barra superior, dock, painel de controle e Arquivos recebem material mais
translúcido e bordas iluminadas. O painel ganha entrada/saída suaves; o dock
preserva abertura/fechamento/hover e amplia o neon. Arquivos realça a linha sob
o ponteiro e não declara mais sua superfície inteira como opaca.

O passe `renderer/backdrop.c` captura somente os nós que precedem a superfície
na ordem de composição. A cena é reduzida a um quarto de cada dimensão usando
um render pass do wlroots; lê-se apenas essa imagem pequena, aplica-se blur
separável em C/Cairo e compõe-se o resultado sob o vidro. A máscara deriva do
alfa da superfície: áreas vazias do dock e do painel não desfocam a tela toda.

O cabeçalho e os clientes nativos Arquivos/camadas do shell registram seus
buffers para esse passe. Não se força transparência no conteúdo de programas
externos. O cabeçalho fornecido pelo compositor nesses programas recebe vidro.
A imagem de fundo agora inclui as janelas atrás, em vez de apenas o wallpaper.

O resultado é reaproveitado enquanto geometria, conteúdo, opacidade e ordem dos
buffers de fundo não mudam. Buffers temporários são liberados após os passes;
a vida do cache acompanha o nó da cena, sem daemon ou loop próprio de efeitos.
Não existe animação infinita de brilho em repouso. Blur, tonalização, reflexo,
sombra e movimento reduzido preservam seus controles existentes.

Limites: leitura GPU→CPU é necessária nesta versão; o filtro é CPU em resolução
reduzida, não um shader de blur. Falha de captura mantém transparência sem blur.
Transformações/escala fracionária e desempenho em vários monitores não foram
medidos. Não há protocolo de sincronização explícita anunciado; os passes usam
a sincronização implícita existente. Não foi feito benchmark comparativo.

## Evidências

- 42 testes release passaram: minimização por clique, Super+M e protocolo xdg,
  restauração por Alt+Tab/visão, preservação do processo/entrada, modo reduzido,
  controles de mídia/volume/Bluetooth, geometria e ciclo de vida.
- `live-backdrop` verifica mistura de pixels de uma superfície atrás, máscara
  transparente, reuso do cache e invalidação por posição; passou com Pixman e
  com GLES2 usando `/dev/dri/renderD128` em backend headless.
- O smoke de minimizar/restaurar também passou com GLES2/headless.
- ASan/UBSan e instalação são registrados após concluir seus checks.
- Prévia de componentes, sem promessa de identidade com imagem anterior:
  [barra](design/neon-glass-panel.png), [painel](design/neon-glass-controls.png),
  [dock](design/neon-glass-dock.png), [Arquivos](design/neon-glass-files.png),
  [moldura](design/neon-glass-frame.png). São renderizações de componentes;
  Arquivos usa pastas de teste, não nomes de arquivos pessoais. A prévia estática
  usa wallpaper em cache; o desfoque entre janelas é verificado nos testes da cena.

A atualização de binários não reinicia o compositor/shell em execução. A sessão
pessoal será preservada e a ativação ocorrerá no próximo login.

## Instalação concluída

Instalado pelo pacman: `jlinux-desktop-git 0.5.0.r130.g267b274-1`, código
`267b274`, em 13/11/2026. Os 42 testes do pacote também passaram. O binário
instalado do compositor foi comparado ao do pacote e o shell instalado gerou
prévia sem erro. Cinco cenários (backdrop, dois modos de minimizar, Arquivos,
painel/dock) passaram com ASan/UBSan e detecção de vazamentos habilitada.

Pacote recuperável anterior:
`jlinux-desktop-git-0.5.0.r129.g7df42e2-1-x86_64.pkg.tar.zst` (cache não localizado)
(se o cache tiver sido movido, localizar o pacote pelo commit `7df42e2`).
A sessão pessoal não foi encerrada. PR #111; sem merge, tag ou release.
O consumo/FPS na sessão de trabalho permanece sem medição.
