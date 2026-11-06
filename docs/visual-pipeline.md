# Animações, efeitos e renderização

A divisão solicitada é `src/animation/`, `src/effects/` e `src/renderer/`. Esses módulos pertencem à implementação nativa. A inspiração Glass é visual; protótipos web continuam separados da tecnologia de execução do desktop.

| Módulo | Responsabilidade | Não deve controlar |
| --- | --- | --- |
| animation/ | Tempo monotônico, curvas, interpolação, interrupção e movimento reduzido | Shaders, buffers ou ativação de efeitos |
| effects/ | Definição de efeitos, parâmetros, dependências de conteúdo e passes necessários | Relógio, agendamento global ou apresentação |
| renderer/ | Recursos gráficos, execução de passes, sincronização, damage e apresentação | Preferências de estilo, curvas ou política adaptativa |

## Fluxo proposto

```text
Preferências do usuário + capacidades + política de desempenho
                              │
                 Estado visual efetivo por superfície
                              │
            animation/ atualiza propriedades no instante t
                              │
            effects/ descreve os efeitos ativos e seus passes
                              │
            renderer/ executa, apresenta e envia frame-done
```

A cena e o agendador do compositor unem essas etapas. Efeito estático não exige loop de animação. Cena parada não deve ser atualizada continuamente. O toolkit poderá compartilhar a biblioteca matemática de animações, sem compartilhar objetos ou ponteiros entre processos.

## Controles independentes

- Animações desativadas: aplicar o estado final das transições; manter efeitos estáticos escolhidos.
- Blur desativado: remover apenas o passe de desfoque; manter sombra, reflexo, tonalização e demais efeitos habilitados.
- Refração desativada: manter transparência, blur e reflexo conforme suas próprias configurações.
- Todos os efeitos desativados: caminho simples, sem passes de pós-processamento; animações geométricas podem continuar.
- Alteração de um efeito: cancelar apenas canais que dependam desse efeito, recompor uma vez a região afetada e preservar transições independentes.

Os efeitos poderão ser configurados globalmente e, quando implementadas regras por superfície, sobrescritos por componente/janela. Um preset Glass inicializa uma combinação; não deve reaplicar valores e apagar alterações individuais do usuário.

O controle adaptativo resolve o estado efetivo sem sobrescrever a preferência persistida. Só reduz os efeitos autorizados e mostra o motivo. Renderer não toma essa decisão sozinho. Redução de movimento, redução de transparência e aumento de contraste são controles separados.

## Recursos e eficiência

Efeito desativado não deve reservar ou atualizar recursos exclusivos desnecessários. Recursos compartilhados permanecem enquanto usados; destruição deve respeitar buffers e trabalho GPU em voo. Blur e sombras expandem a região de damage; refração precisa considerar o conteúdo amostrado. Tamanhos de kernel e caches exigem limites e medição.

Um efeito que amostra o fundo pode exigir composição e impedir direct scanout naquele quadro. O renderer deve testar elegibilidade e usar fallback correto; não prometer scanout direto com toda combinação de Glass. Medir separadamente o custo dos efeitos e o custo de animar seus parâmetros.

## Estado real

Nesta entrega, renderer.c passou a possuir a criação/limpeza do renderizador e alocador wlroots e a apresentação/frame-done, extraídas de compositor/main.c. O comportamento gráfico existente foi preservado.

animation/ continua sendo um contrato sem motor. effects/glass.c agora compõe o primeiro material do painel/dock em Cairo: amostragem do wallpaper, tonalização e reflexos estáticos de borda. renderer/blur.c executa um box blur separável em imagem ARGB32 reduzida a 1/4 por dimensão. O shell mantém esse fundo em cache até a resolução mudar; hover e relógio não recalculam o blur.

Limite explícito: o fundo amostrado é apenas o wallpaper procedural do shell, não janelas de outros clientes. Não há refração, sombra difusa, shaders Glass ou adaptação automática. A rasterização permanece em CPU; este caminho inicial não substitui o futuro pipeline GPU do compositor. As referências do usuário continuam sendo a meta, sem fidelidade completa.

Controles iniciais de processo, independentes, aceitam somente `0`/`1`: `JLINUX_GLASS_BLUR`, `JLINUX_GLASS_TINT`, `JLINUX_GLASS_REFLECTION`. Ausência significa ativado. Exemplo: `JLINUX_GLASS_BLUR=0 build-native/jlinux-session --headless --no-terminal --duration 2`. Desativar blur mantém o cache reduzido do wallpaper para os demais efeitos; não há passe de blur. Desativar tint/reflection preserva a amostragem do fundo. Ainda não há persistência ou recarga ao vivo; TOML continua exemplo não carregado.

## Molduras de janelas e pontos de brilho

O compositor agora compartilha effects/glass.c e toolkit/painter.c com o shell. renderer/window-frame.c desenha cabeçalho de 36 px e bordas de 6 px em quatro buffers ARGB32 estreitos, liberados por referências wlroots. O interior da janela não recebe buffer de decoração. Mudança de tamanho/título atualiza as faixas; conteúdo novo do cliente com mesma geometria não as redesenha. Foco altera opacidade da moldura sem rasterizar de novo.

As molduras usam transparência real sobre a cena, tonalização e reflexos estáticos; **não têm blur de fundo**. A opção BLUR continua limitada ao wallpaper do shell, sem efeito nas molduras. TINT/REFLECTION são compartilhadas e independentes. Conteúdo e decorações próprias de aplicativos que não negociam xdg-decoration não são retematizados; pode haver cabeçalho adicional. Cantos externos da moldura são arredondados; o conteúdo do cliente não recebe máscara arredondada.

A barra superior recebe três reflexos estreitos localizados junto ao contorno, visíveis com REFLECTION ativado. Não há animação, timer adicional ou movimento automático dos brilhos.

Sombreamento interno das molduras: `JLINUX_GLASS_SHADOW=0/1` (padrão ligado) controla um traço escuro interno em passe próprio, independente de REFLECTION. Não cria sombra externa nem blur. O shell valida essa preferência compartilhada, mas não aplica esse sombreado ao dock/painel.

### Acabamento contínuo atual das janelas

O pedido de borda neon discreta substitui os quatro pontos de brilho da moldura: gradiente azul/violeta, traço de 0,85 px e halo interno recortado de 2,8 px. A geometria segue 3 px nas laterais/inferior. REFLECTION controla esse acabamento; SHADOW permanece independente. Material de janela não chama os reflexos pontuais do dock/painel; esses componentes permanecem intactos. Renderização continua por eventos, em faixas estreitas, sem loop novo.
