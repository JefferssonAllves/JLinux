# Direção visual: Glass no JLinux

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

## Correção de paleta e proposta V2

A preferência mais recente é base escura e pouco chamativa, predominantemente grafite, carvão e vidro fumê. Roxo/rosa são acentos sutis em algumas bordas e indicadores, não cores principais do fundo, painéis ou janelas. Preservar o aspecto futurista detalhado com reflexos, profundidade e materiais, sem brilho colorido excessivo.

A [proposta V2](design/README.md) mantém dock à esquerda e a barra superior. Substitui a direção de paleta da V1, preservada apenas para comparação. V2 ainda não foi aprovada; não há mudança na interface em execução.
