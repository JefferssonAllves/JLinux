# Animações, efeitos e renderização

**Estado atual:** o dock lateral antigo permanece removido. O pedido mais recente autoriza um novo dock circular por Super; ver [futurista.md](futurista.md). Menções ao antigo dock lateral abaixo são histórico.


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

animation/ implementa a primeira transição escalar smoothstep, reversível, usada na abertura/fechamento e destaque do dock; o chamador fornece tempo monotônico e o shell agenda frames Wayland somente durante a transição. O compositor também usa essa transição nas janelas; adaptação automática de efeitos permanece pendente. effects/glass.c agora compõe o primeiro material do painel/dock em Cairo: amostragem do wallpaper, tonalização e reflexos estáticos de borda. renderer/blur.c executa um box blur separável em imagem ARGB32 reduzida a 1/4 por dimensão. O shell mantém esse fundo em cache até a resolução mudar; hover e relógio não recalculam o blur.

Limite explícito: o fundo amostrado é apenas o wallpaper procedural do shell, não janelas de outros clientes. Não há refração, sombra difusa, shaders Glass ou adaptação automática. A rasterização permanece em CPU; este caminho inicial não substitui o futuro pipeline GPU do compositor. As referências do usuário continuam sendo a meta, sem fidelidade completa.

Controles iniciais de processo, independentes, aceitam somente `0`/`1`: `JLINUX_GLASS_BLUR`, `JLINUX_GLASS_TINT`, `JLINUX_GLASS_REFLECTION`. Ausência significa ativado. Exemplo: `JLINUX_GLASS_BLUR=0 build-native/jlinux-session --headless --no-terminal --duration 2`. Desativar blur mantém o cache reduzido do wallpaper para os demais efeitos; não há passe de blur. Desativar tint/reflection preserva a amostragem do fundo. Ainda não há persistência ou recarga ao vivo; TOML continua exemplo não carregado.

## Molduras de janelas e pontos de brilho

O compositor agora compartilha effects/glass.c e toolkit/painter.c com o shell. renderer/window-frame.c desenha cabeçalho de 36 px e bordas de 6 px em quatro buffers ARGB32 estreitos, liberados por referências wlroots. O interior da janela não recebe buffer de decoração. Mudança de tamanho/título atualiza as faixas; conteúdo novo do cliente com mesma geometria não as redesenha. Foco altera opacidade da moldura sem rasterizar de novo.

As molduras usam transparência real sobre a cena, tonalização e reflexos estáticos; **não têm blur de fundo**. A opção BLUR continua limitada ao wallpaper do shell, sem efeito nas molduras. TINT/REFLECTION são compartilhadas e independentes. Conteúdo e decorações próprias de aplicativos que não negociam xdg-decoration não são retematizados; pode haver cabeçalho adicional. Cantos externos da moldura são arredondados; o conteúdo do cliente não recebe máscara arredondada.

A barra superior recebe três reflexos estreitos localizados junto ao contorno, visíveis com REFLECTION ativado. Não há animação, timer adicional ou movimento automático dos brilhos.

Sombreamento interno das molduras: `JLINUX_GLASS_SHADOW=0/1` (padrão ligado) controla um traço escuro interno em passe próprio, independente de REFLECTION. Não cria sombra externa nem blur. O shell valida essa preferência compartilhada, mas não aplica esse sombreado ao dock/painel.

### Acabamento contínuo atual das janelas

O pedido de borda neon discreta substitui os quatro pontos de brilho da moldura: gradiente azul/violeta, traço de 0,85 px e halo interno recortado de 2,8 px. A geometria segue 3 px nas laterais/inferior. REFLECTION controla esse acabamento; SHADOW permanece independente. Material de janela não chama os reflexos pontuais do dock/painel; esses componentes permanecem intactos. Renderização continua por eventos, em faixas estreitas, sem loop novo.

## Integração do material pendente

`jlinux_glass_panel_paint` aplica a máscara de 16 px ao material compartilhado, com grupo Cairo restrito ao painel; a reserva superior fica em 52 px e somente os 36 px de controles recebem entrada. `jlinux_glass_window_tint` substitui o preenchimento genérico do cabeçalho quando TINT está ativo, preservando transparência e os quatro buffers estreitos da moldura. REFLECTION controla separadamente o contorno mais nítido e seu toque rosado. São os helpers já validados nas branches visuais, agora combinados com a base instalada sem dock; não há novo loop ou blur de janelas.


### Transições de janelas

`interaction.c` amostra `animation/transition.c` pelo relógio monotônico nos frames das saídas. Entrada: fade e deslocamento de 12 px em 180 ms. Durante arraste: elevação de 3 px e halo em 120 ms, retorno em 150 ms; a posição lógica acompanha o cursor diretamente. Saída: cópia de nós de cena mantendo referências aos buffers, fade/deslocamento de 8 px em 160 ms. Captura antes do listener de unmap da cena; os recursos temporários pertencem ao compositor, sem ponteiros ao cliente, e são liberados ao terminar, trocar workspace ou destruir o estado. `wlr_buffer_finish` encerra buffers próprios antes de liberar Cairo.

O encaixe compartilha um único cálculo de área entre prévia e posicionamento, respeitando reserva da barra e limites do cliente. A prévia cresce 12 px por borda e aparece em 160 ms; saída em 120 ms. Somente soltar altera o tamanho da janela. Prévia e ghosts não recebem entrada. Não há timer/polling de animação: um novo frame é solicitado enquanto há transições ativas. `JLINUX_REDUCED_MOTION=1` aplica os estados finais sem deslocamentos nem ghost, mantendo as escolhas estáticas de efeitos.

`effects/window.c` pinta sombra suave e halo azul-violeta. `renderer/window-effects.c` mantém oito tiras externas estreitas em cache por tamanho/opções, sem imagem transparente do tamanho inteiro da janela. Sombra segue `JLINUX_GLASS_SHADOW`; halo segue `JLINUX_GLASS_REFLECTION`; cor segue `JLINUX_GLASS_TINT`. Fullscreen não recebe esses ornamentos. Isso não implementa blur/refração ao vivo. Os snapshots usam sincronização implícita, conforme os protocolos atualmente publicados; suporte a fences deverá preceder a publicação de explicit-sync.

### Transição de geometria no encaixe lateral

O controlador fornece progresso smoothstep de 220 ms pelo motor existente. renderer/window-effects.c interpola posição e tamanho das referências de buffers, sem relógio próprio ou novas imagens Cairo por quadro. O cliente recebe um único configure para o destino; conteúdo vivo fica oculto até o handoff final ou interrupção. Ghosts não recebem eventos; clicar outra janela não cancela as demais transições. Cada view possui seu snapshot e o libera em finalização, mudança de contexto, unmap/destroy e teardown. Redução de movimento ou falta de recursos usam o caminho imediato. Não altera os controles de sombra/reflexo nem introduz timer de animação.

### Visão de janelas

renderer/overview.c realiza captura reduzida/desfoque em cache e cartões usando helpers existentes; interaction.c possui o modo modal, referências às views e seleção. Cada página possui no máximo nove snapshots e o fundo é capturado somente ao abrir. A composição para captura usa renderer/allocator da saída e sincronização implícita já adotada pelo compositor. Captura, texturas temporárias e estado de output são liberados antes de retornar; buffers dos cartões seguem referências wlroots. Não há caminho de pixels pelo shell ou serviço externo. Blur desativado não captura nem mantém cache exclusivo. A visão não adiciona relógio ou loop de animação.

O contorno cromático dos setores do dock reutiliza `effects/glass.c:jlinux_glass_rim`, também chamado pelas reflexões das janelas. O helper desenha o path fornecido, sem relógio/loop próprio. O halo e as duas camadas de borda acompanham o progresso das transições já existentes; nenhuma animação contínua nova foi adicionada.

O degradê do dock é executado por `jlinux_dock_material` (eixo externo→interno dos setores) e `jlinux_dock_center_material` (centro radial), em effects/dock.c. A reflexão acetinada e o halo de contorno são passes de efeitos, sem controlar tempo. A geometria e os ícones permanecem no shell, e as curvas de hover continuam em animation/. Não se usa o mockup como superfície interativa.
