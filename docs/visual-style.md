# Direção visual: Glass no JLinux

**Estado atual:** o dock lateral foi removido por pedido do usuário, incluindo ações, seletor, configuração e sobreposição. Menções ao componente nas iterações abaixo são histórico visual; não devem orientar sua reintrodução.


## Referência principal atual

O usuário rejeitou as propostas geradas anteriormente e forneceu duas imagens no commit cf4238a, “Estilo que eu quero”. Pediu reprodução fiel dessas fotos. A [galeria de referências](design/README.md) passa a definir o visual: material azul-preto translúcido, bordas finas iluminadas e reflexos azuis/violetas localizados, ícones coloridos, dock à esquerda e composição das fotos. As seções de propostas anteriores abaixo são histórico e não prevalecem sobre essas referências.

Não criar uma nova interpretação cinza/prateada nem recolorir o desktop inteiro de roxo. O próximo protótipo deve ser comparado diretamente às imagens do usuário, antes da implementação nativa definitiva.

Preferência do usuário: ambiente inspirado no vidro dos dispositivos Apple, com identidade e parâmetros próprios do JLinux. Esta é a direção para os próximos protótipos; não uma afirmação de que o desktop atual já apresenta Glass.

A referência é Apple Liquid Glass: materiais para navegação e controles, profundidade e separação legível do conteúdo. A orientação da Apple prioriza uso seletivo e adaptação a preferências de contraste e transparência. Referência primária: https://developer.apple.com/design/human-interface-guidelines/materials.

## Prototipar antes da implementação nativa

Explorar painel, dock e popovers com superfícies translúcidas, contornos iluminados e sombras suaves. Comparar fundo claro, escuro e detalhado; manter texto e ícones nítidos. Prototipar refração e reflexos separadamente para avaliar se contribuem ao visual desejado. Evitar aplicar efeitos caros a toda a área de conteúdo por padrão.

Ferramentas web são usadas para discutir aparência e interação com o usuário. Após o visual ser acordado, implementar os materiais no pipeline nativo. APIs SwiftUI/UIKit descritas pela skill Liquid Glass não são APIs Linux.

## Composição configurável do material

| Parte visual | Controle independente planejado |
| --- | --- |
| Superfície translúcida | Transparência e opacidade |
| Desfoque do fundo | Blur, raio e qualidade |
| Cor do vidro | Tonalização e intensidade |
| Volume e separação | Sombra e parâmetros geométricos |
| Contorno luminoso | Reflexo de borda e intensidade |
| Distorção óptica | Refração e intensidade |
| Resposta ao movimento | Animações e curvas, no motor separado |

Mesmo sem blur ou refração, o tema deve oferecer um fallback legível. Movimento reduzido não remove automaticamente o material; redução de transparência não deve desativar transições geométricas. O usuário poderá escolher desde superfícies sólidas até a combinação de efeitos desejada, dentro das capacidades implementadas.

No hardware com Radeon integrada, medir largura de banda, memória, tempo de quadro e energia por efeito. Refração e materiais dependentes do fundo entram depois da linha de base e da validação do caminho simples. Nenhuma redução automática deve alterar silenciosamente o gosto do usuário.

## Proposta V1 para avaliação

O usuário definiu dock vertical à esquerda, barra superior com Wi-Fi/Bluetooth e menu Suspender/Reiniciar/Desligar, visual Glass e detalhes predominantes em roxo/rosa. A correção mais recente de posição é **à esquerda**.

A [imagem V1](design/README.md) é um estudo visual gerado para avaliação pelo celular via GitHub. Não é screenshot do desktop atual nem visual aprovado. Próxima implementação deve considerar o feedback do usuário antes de reproduzir a composição nativamente.

## Ajuste solicitado de material do dock e barra

O usuário pediu concentrar a iteração no dock e na barra superior, deixando o wallpaper em segundo plano. O material inicial agora usa tonalização azul-acinzentada mais clara, reflexos estáticos azuis/violetas localizados e iluminação interna do contorno. Botões deixam mais material visível. A referência continua sendo a foto do usuário; este ajuste não significa fidelidade exata ou aprovação. Blur segue limitado ao wallpaper, conforme visual-pipeline.md.

### Barra compacta e acabamento de bordas

Após feedback positivo sobre o material mais claro, a barra passa a 44 px de altura e 6 px de margem superior/lateral. O contorno usa iluminação direcional para destacar o volume do vidro, com linha interna menos marcada. Dock mantém tamanho e ações; wallpaper permanece igual. Prévia nativa atualizada; fidelidade completa segue pendente.

### Dock compacto

Dock com 56 px de largura, 6 px de margem esquerda e botões de 40×40. Detalhes azuis/violetas das bordas e reflexos localizados recebem mais brilho, preservando a tonalização clara e a barra de 44 px. Prévia e geometria de execução compartilham constantes; wallpaper preservado.

### Refinamento da barra superior

Altura atual: 36 px, margem de 6 px. A extremidade direita usa azul-violeta menos rosado e menos intenso, com reflexo localizado suavizado. O dock mantém seu material e geometria anteriores. A ponta esquerda da barra mantém o brilho reduzido solicitado.

### Molduras nativas

Cabeçalho compacto de 36 px, título, fechar, transparência e bordas azul-violeta do material compartilhado. Janela sem foco recebe moldura mais discreta. Arraste pelo cabeçalho e resize pelas bordas esquerda/direita/inferior; Super+arraste permanece. Maximizar/restaurar estão implementados pelo botão da moldura e Super+↑/Super+↓; minimizar, tema interno dos aplicativos e blur sobre janelas seguem pendentes. A barra superior ganhou reflexos estreitos em três pontos do contorno, conforme novo pedido do usuário.

### Forma das janelas definida pelo usuário

O cabeçalho das janelas permanece retangular, com vidro semelhante ao dock. Contorno iluminado azul-violeta e cantos externos com raio discreto de 6 px, sem formato de cápsula. A junção inferior do cabeçalho é reta. Essa regra vale para a moldura comum dos xdg-toplevel; o interior dos aplicativos não recebe tema forçado.

### Maximização e área útil

O cabeçalho aprovado ganha botão maximizar/restaurar junto ao fechar. Maximizar mantém a moldura e ocupa a área útil restante da saída central, respeitando barra e dock; restaurar recupera posição/tamanho e mantém o cabeçalho acessível caso a área útil tenha mudado. O dock fica centralizado na área abaixo da barra. Multimonitor completo continua pendente.

### Profundidade adicional nas molduras

Cabeçalho retangular e cantos de 6 px preservados. Reflexo vertical em camadas, dois pontos de luz no topo e dois nas bordas laterais reforçam o aspecto de vidro. Contorno interno fino realça espessura. O acabamento adicional pertence a REFLECTION e não altera o material do dock/barra ou o conteúdo do aplicativo. São reflexos estáticos estilizados, não simulação óptica ou blur.

### Acabamento contínuo das bordas

Após pedido para aproximar as bordas ao dock, a moldura passa a reutilizar o mesmo gradiente de contorno do dock, com faixa suave contínua e linha interna. Os clarões extras isolados nas laterais foram removidos. Cabeçalho retangular e cantos de 6 px preservados. A equivalência de paleta não significa blur idêntico: as molduras ainda usam transparência sem desfoque de fundo.

### Correção após rejeição da faixa grossa

O usuário rejeitou o contorno largo contínuo. Direção atual: bordas laterais/inferior de 3 px, linha discreta de 0,8 px entre quatro pontos intensos, núcleo quase branco e reflexão azul/violeta localizada. Cabeçalho retangular e raio discreto de 6 px preservados. A faixa larga da iteração anterior foi removida; dock/barra permanecem como estavam. Não declarar fidelidade exata à foto sem aprovação.

### Sombreamento e contorno superior fino

Sombra interna leve destaca o volume da moldura de 3 px sem engrossá-la. É controlada por `JLINUX_GLASS_SHADOW`, independentemente dos reflexos; não é sombra externa desfocada. Barra superior e cabeçalhos compactos usam contorno de 0,75 px, bevel reduzido e nenhuma linha interna duplicada, mantendo altura e pontos de brilho. Dock preservado.

## Bordas neon discretas — feature/window-neon-rim

Pedido atual substitui pontos focais nas janelas: contorno contínuo de 0,85 px azul/violeta, halo interno discreto de 2,8 px recortado à moldura, sem núcleos brancos. Cabeçalho retangular de 36 px, borda física de 3 px e cantos de 6 px preservados. Dock/barra superior não mudam. Gerenciador recebe fundo opaco em gradiente azul-ardósia mais claro e barra interna com o mesmo acabamento contínuo; não altera conteúdo de aplicativos externos.

Quatorze testes locais release e ASan/UBSan passaram. Teste da moldura passa a conferir continuidade de opacidade e variação cromática em vez dos antigos pontos intensos; controles independentes preservados. Prévias dos componentes inspecionadas junto às referências, sem declarar correspondência exata. Sem timer, animação, blur novo ou benchmark. Ativação das molduras exige novo compositor no próximo login; sessão atual preservada devido à investigação STABILITY-BOOT.


## Ajuste dark e tipografia compacta

O toolkit nativo aplica 93% do tamanho nominal dos textos (aproximadamente 7% menor), compartilhado por shell, molduras, Arquivos e greeter. Não configura fontes do terminal nem de aplicativos externos. O material compartilhado usa tonalização azul-preta mais escura e menor reflexão ampla sobre as superfícies, com contornos azuis/violetas mais saturados e intensos. A espessura dos contornos permanece igual. Arquivos recebe também um gradiente de fundo mais escuro; wallpaper e ícones permanecem preservados.

Os controles independentes de tint/reflection continuam respeitados. São novos valores padrão compilados, sem editor de tema ou recarga ao vivo. Não comprovam fidelidade às fotos nem desempenho de GPU. Prévias do shell, Arquivos e moldura foram inspecionadas; comparação antes/depois de Arquivos na mesma resolução mostrou a redução da fonte e o escurecimento das superfícies.

## Consolidação aplicada aos pedidos mais recentes

A integração `feature/apply-pending-updates` reúne o cabeçalho preto translúcido (88%), contorno nítido (1,6 px), halo reduzido (2,2 px a 18%) e o rosado de #54 — RGB (.84, .24, .91) no stop antes violeta. A barra superior ocupa toda a largura no topo, com cantos retos e 16 px de fade abaixo dos 36 px de controles. Dock e rótulos de menus removidos permanecem ausentes. Inventário e precedência dos pedidos em [applied-updates.md](applied-updates.md); versões anteriores descritas acima são histórico.

## Referência Futurista mais recente

Por pedido do usuário, a tela de Downloads “Central de Controle JLinux Futurista.png” orienta a nova composição: dock circular central acionado por Super, centro de controle à direita e barra arredondada azul-violeta. Substitui a orientação de ausência de dock para esta implementação; não restaura o dock lateral antigo. Código, prévia, comparação e limites em [futurista.md](futurista.md). O usuário autorizou instalar a prévia completa sem exigir agora reconstrução exata dos assets ausentes.

## Barra superior compacta atual

O pedido atual prevalece sobre o formato arredondado da referência Futurista: 36 px, largura total, cantos retos, sem marca JLinux e sem ícone de volume. Hora centralizada, bateria com porcentagem e raio de carregamento condicionado ao estado real; ajustes mantidos. Dock radial e animação de expansão central preservados.


### Movimento e profundidade das janelas

Entrada/saída curtas com fade e deslocamento vertical; arraste com elevação discreta e halo azul-violeta externo. Sombra difusa escura separa janelas sobrepostas, sem alterar o conteúdo dos aplicativos. Nas bordas laterais, contorno violeta com preenchimento translúcido indica a metade disponível abaixo da barra antes de soltar. Movimento reduzido mantém os estados finais e materiais estáticos. Tempos e controles independentes em [visual-pipeline.md](visual-pipeline.md).

[Prévia dos componentes nativos](design/window-transitions.png), 1280×720: renderização das mesmas tiras de moldura/halo/prévia, com conteúdo de janela ilustrativo. Não é captura da sessão física nem comprova fluidez em GPU; não altera as referências principais do usuário.

### Data/hora com acabamento das molduras

Bloco Glass de 28 px, raio de 5 px e texto central de 14 px. Preenchimento e contorno usam diretamente os helpers de janela, com tonalização, sombra e reflexão controladas separadamente. A barra externa segue retangular, com 36 px; bateria e ajustes mantidos.

O centro de controle reutiliza o material de src/effects/dock.c e os contornos de janela, incluindo cartões e controles ativos. Prévia: [painel padronizado](design/control-center-standard.png).
