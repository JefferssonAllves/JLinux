# Direção visual: Glass no JLinux

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

### Maior realce nas cores das janelas

Nesta iteração, o cabeçalho ganha reflexo azul mais intenso no topo e violeta na base. O contorno usa azul/ciano e violeta mais vivos, com maior opacidade no halo interno existente. Mantidas as espessuras de 0,85 px e 2,8 px, as bordas físicas de 3 px e o fundo escuro. O material compartilhado também atualiza a barra interna de Arquivos e o acabamento do login; painel/dock usam outro caminho de pintura.

REFLECTION continua controlando esse realce independentemente de TINT, SHADOW e BLUR. São apenas valores de cor/opacidade: nenhum passe, buffer, timer ou animação adicional. Não é alteração das cores do conteúdo de aplicativos externos.

Prévias nativas de moldura e Arquivos atualizadas e inspecionadas antes/depois. Uma prévia temporária de moldura também foi renderizada a 1670×941 para comparação com a primeira referência do usuário: contorno azul/violeta e fundo escuro preservados; materiais, composição e proporções da referência ainda não são reproduzidos integralmente. Não é captura da sessão ativa nem promessa de fidelidade exata.

### Barra superior encaixada no topo

O pedido mais recente substitui a cápsula flutuante da barra: largura total da saída, margem zero no topo/laterais e cantos retos. Os controles continuam em 36 px; uma faixa adicional de 16 px reduz progressivamente a opacidade do material até zero. O material é desenhado com seu contorno fora da área visível, sem linha inferior. O degradê mistura a base ao wallpaper mantendo o vidro azul-preto e reflexos estáticos.

A superfície e a reserva de área útil passam a 52 px. Só os 36 px de controles recebem entrada; a faixa de transição não captura cliques. Dock permanece com sua forma e material, centralizado na área abaixo dessa nova reserva. Menus de energia/volume e botões transitórios mantêm suas posições anteriores.

BLUR, TINT e REFLECTION continuam independentes. O fundo de vidro ainda amostra somente o wallpaper em cache, não janelas. A máscara usa um grupo Cairo temporário restrito à barra quando há redesenho; não adiciona timer ou animação. Não há medição de desempenho nesta mudança.

### Cabeçalho mais preto e borda mais definida

O novo pedido escurece o fundo do cabeçalho: preto levemente azulado com opacidade de 88%, mantendo transparência e cantos existentes. A tonalização pertence a TINT e substitui a pintura anterior nesse trecho, sem passe adicional. REFLECTION deixa reflexos mais discretos no corpo para destacar o contorno ciano/azul/violeta saturado.

O traço de contorno passa de 0,85 para 1,6 px, com halo interno reduzido de 2,8 para 2,2 px e opacidade de 40% para 18%. A borda física continua em 3 px. A intenção é uma linha colorida mais nítida, com menos luz difusa; não aumenta a moldura nem adiciona blur. O material de reflexos também é usado pela barra de Arquivos e pelo login. A barra superior reta com fade e o dock continuam com seu material anterior.

Prévias de moldura/Arquivos inspecionadas antes/depois; uma prévia temporária a 1670×941 foi comparada à primeira referência, sem afirmar reprodução integral das fotos. Não é captura da sessão ativa. O conteúdo de aplicativos externos permanece sob controle deles.
