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
