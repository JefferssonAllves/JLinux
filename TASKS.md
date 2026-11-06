# Plano central do JLinux

## Navegador visível no dock — fix/visible-browser-dock

O ID `org.jlinux.Browser.desktop` já estava fixado, mas ficava fora das três posições visíveis após iniciar o shell. O dock passa a quatro posições/292 px: Arquivos, Terminal, Codex e o primeiro app fixado aparecem juntos. A roda continua acessando apps além do primeiro; botões vazios não executam comandos. Tests de clique/posição atualizados para o shell e modo `--dock-panel`, verificando que o atalho persistido abre sem rolar. Preferências e instalação separada do navegador preservadas. Instalação/ativação e resultados finais serão registrados após os checks.

Este arquivo é a fonte central das tarefas iniciais. Personalização ampla, compositor próprio, shell separado e eficiência mensurada orientam todas as entregas. Responsável pela execução: coordenador Codex; perfis abaixo indicam especialidade, não execução paralela automática.

Estados: **concluído** = implementação e evidências indicadas; **em andamento** = trabalho ativo; **pendente** = não entregue. Uma tarefa pode estar concluída na branch sem estar integrada em develop/main. Cada entrega registra PR, validação e limites. CI verde não é teste de GPU.

## Diretriz para as próximas telas

As ferramentas web servem para criar e iterar protótipos visuais antes da interface definitiva nativa. O shell GTK/CSS foi substituído pela primeira base Wayland/C; o toolkit ainda é inicial. Tecnologias de produção serão escolhidas por adequação e desempenho medido, preservando personalização.

| ID | Prioridade / estado | Tarefa | Dependências | Critério de conclusão |
| --- | --- | --- | --- | --- |
| DESIGN-01 | P1 · pendente | Prototipar e validar a próxima tela com o usuário | Direção visual do usuário | Protótipo identificado como tal, feedback incorporado e visual acordado antes da implementação nativa definitiva |
| NATIVE-01 | P1 · pendente | Definir a implementação nativa do visual acordado | DESIGN-01, PERF-01 | Decisão documentada com critérios e evidências; plano de substituição do shell provisório pelo toolkit próprio, sem transportar dependências web para o desktop |

## Preparação e primeira interface

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| BASE-01 | P0 · concluído | Inventariar hardware e decidir base | — | coordenador | docs/hardware.md e docs/architecture.md; capacidades e incertezas explícitas |
| BASE-02 | P0 · concluído | Núcleo Wayland próprio | BASE-01 | compositor-engineer | Compila; saída e socket headless; SIGTERM limpa recursos; smoke test passa |
| DEV-01 | P0 · pendente de integração | Formalizar branches e PRs | BASE-02 | coordenador | PR #1 validado e integrado por autorização; sem push direto em branches duradouras |
| DEV-02 | P0 · concluído na branch | Configurar perfis, skills e ferramentas | DEV-01 para integração | coordenador | PR #2; TOML/skills validados e fontes registradas; plugins sem conexão identificados |
| UI-01 | P0 · concluído na branch | Desktop visual mínimo | BASE-02 | visual-engineer + compositor-engineer | Fundo, barra e relógio em processo separado; release/ASan/UBSan e teste de reinício headless passaram; GPU/visual físico pendentes |
| TEST-01 | P0 · pendente | Isolar execução e instalação | BASE-02 | correctness-reviewer | Runtime temporário e cleanup; prefixo de desenvolvimento; documentar execução aninhada e recuperação sem substituir sessão cotidiana |
| PERF-01 | P0 · pendente | Linha de base de desempenho | UI-01, TEST-01 | performance-reviewer | Registrar commit/backend, PSS, CPU ociosa e workload; distinguir medição headless de GPU; salvar metodologia e resultados reproduzíveis |

## Tornar o desktop interativo

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| WM-01 | P0 · parcial na branch | xdg-shell e janelas | BASE-02 | compositor-engineer | Configure/map/unmap/destroy e popups; múltiplos clientes reais; desconexão sem crash |
| INPUT-01 | P0 · parcial na branch | Teclado e ponteiro | WM-01 | compositor-engineer | Seat, keymap, movimento, cliques, foco e hotplug; não perder entrada após fechar janela |
| WM-02 | P1 · parcial na branch | Mover, redimensionar e atalhos | WM-01, INPUT-01 | compositor-engineer | Interações com validação de serial; fechar janela, alternar foco e encerrar sessão |
| SHELL-01 | P1 · pendente | Lançador e painel interativos | UI-01, INPUT-01, WM-01 | visual-engineer | Abrir aplicativos .desktop sem shell interpolation; estados vazios e falhas visíveis |
| WM-03 | P1 · pendente | Espaços de trabalho e regras | WM-02 | compositor-engineer | Regras por app, troca e movimentação de janelas; foco consistente e configuração testada |

## Personalização e movimento

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| CONFIG-01 | P1 · pendente | Parser TOML e modelo versionado | UI-01 | visual-engineer | Ler arquivo real; validar cores/limites; erro preserva último estado válido; testes de entradas inválidas |
| THEME-01 | P1 · pendente | Temas e aplicação ao vivo | CONFIG-01 | visual-engineer | Tokens de cor, fonte e geometria; aplicar/desfazer sem reiniciar compositor; contraste e escala verificados |
| ANIM-01 | P1 · pendente | Motor próprio de animações | WM-02, CONFIG-01 | visual-engineer | Tempo monotônico, curvas, interrupção e movimento reduzido; testes determinísticos sem GPU; repouso sem timer contínuo |
| TOOLKIT-01 | P2 · pendente | Biblioteca visual mínima | THEME-01, INPUT-01 | visual-engineer | Componentes reutilizados no shell; foco, texto/IME e acessibilidade planejados; tipografia via biblioteca especializada |
| EFFECT-01 | P2 · pendente | Sombras, opacidade e blur | ANIM-01, PERF-01 | visual-engineer | Efeitos independentes configuráveis; custo por efeito registrado; desativação restaura caminho simples |

## Eficiência e estabilidade

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| PERF-02 | P1 · pendente | Damage e agendamento por eventos | UI-01, PERF-01 | compositor-engineer | Sem redraw contínuo de cena parada; medir despertares; animações e clientes continuam recebendo quadros corretamente |
| GPU-01 | P1 · pendente | DMA-BUF e sincronização | WM-01 | compositor-engineer | Importação e feedback de formatos; sincronização/lifetime testados; fallback SHM; validar na Radeon real |
| GPU-02 | P2 · pendente | Verificar direct scanout | GPU-01, PERF-01 | performance-reviewer | Evidenciar elegibilidade e uso em DRM; fallback correto quando efeitos/overlays exigem composição |
| PERF-03 | P2 · pendente | Adaptação de efeitos e energia | EFFECT-01, PERF-01, CONFIG-01 | performance-reviewer | Orçamento por saída, histerese, modo fixo/adaptativo e motivo observável; respeitar permissões visuais |
| TEST-02 | P1 · pendente | Sanitizers e falhas de ciclo de vida | WM-01 | correctness-reviewer | Builds separados ASan/UBSan; testar cliente/saída removidos e inicialização parcial; triagem reproduzível |
| TEST-03 | P2 · pendente | Fuzzing de parsers/APIs isoláveis | CONFIG-01, TEST-02 | correctness-reviewer | Harness limitado e determinístico, corpus e reprodução de crash; sem campanha infinita |
| OUTPUT-01 | P1 · pendente | Múltiplas saídas e escala | WM-01, INPUT-01 | compositor-engineer | Hotplug, reposicionamento e escala sem superfícies órfãs; teste físico documentado |

## Antes de usar como sessão cotidiana

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| SESSION-01 | P2 · pendente | Bloqueio, suspensão e retomada | INPUT-01, OUTPUT-01 | compositor-engineer | Protocolo de bloqueio adequado; não expor desktop durante bloqueio; testar retomada real |
| SYSTEM-01 | P2 · pendente | Áudio, rede, energia e notificações | SHELL-01 | visual-engineer | Integração com serviços existentes; erros e serviços indisponíveis tratados |
| COMPAT-01 | P2 · pendente | Portais, captura e acessibilidade | WM-01, SHELL-01 | compositor-engineer | Permissões e protocolos documentados; captura e navegação assistiva verificadas; Xwayland opcional avaliado |
| RELEASE-01 | P3 · pendente | Pacote Arch e entrada de login | SESSION-01, SYSTEM-01, COMPAT-01, TEST-02 | coordenador | Instalação/remoção rastreáveis, sessão atual preservada; release somente após autorização e evidências |

## Próxima entrega ativa

**UI-01 entregue na branch `feature/basic-desktop`:** fundo e barra superior, identidade JLinux e relógio, tema CSS adicional e execução com runtime isolado. `Desktop 01` é um rótulo estático, não espaços de trabalho implementados. Shell GTK temporário; toolkit próprio continua em TOOLKIT-01. Validação em docs/validation.md.

Próximas tarefas: validar o visual aninhado no hardware real, iniciar PERF-01 para estabelecer linha de base e implementar WM-01/INPUT-01 para tornar o desktop interativo. Não antecipar efeitos caros ou adaptação automática antes das medições.

## Entrega interativa inicial

Branch `feature/terminal-and-input`: Foot abre automaticamente, recebe comandos e pode executar Codex. Mouse/teclado físicos usam libinput/XKB; foco por clique, Alt+Tab, Super+Enter, Super+Q, Super+Esc e Super+arraste estão implementados. Botões Terminal/Codex no shell. Runner com DRM explícito, runtime isolado, grupos de processos e espera por pidfd.

Teste headless de dois terminais reais cobre digitação, mudança de foco por clique, desconexão da janela focada e saída por atalho. Release e ASan/UBSan passaram. Smoke DRM curto também executou; interação física do usuário e comportamento prolongado continuam sem confirmação. WM-01/INPUT-01/WM-02 permanecem parciais: popups avançados, hotplug físico e movimentação por pedidos das decorações não foram validados/implementados integralmente. Próximo passo: feedback do usuário no desktop e baseline PERF-01.

Sessão interativa iniciada automaticamente no hardware após CI verde do PR #6, com Codex ativo dentro do Foot. Radeon, monitor e periféricos físicos foram detectados nos logs; confirmação manual do usuário continua pendente. Evidência detalhada em docs/validation.md.

## Pipeline visual e Glass

- **VISUAL-STRUCTURE · concluído na branch:** diretórios animation/, effects/ e renderer/ separados; código existente de criação/limpeza/apresentação movido para renderer/. Os outros dois módulos ainda são contratos documentados, não motores implementados.
- **DESIGN-01 · pendente:** próximo protótipo segue a inspiração Glass, com validação em fundos claro, escuro e detalhado; usuário escolhe a aparência antes da implementação nativa.
- **EFFECT-02 · pendente; depende de CONFIG-01 e EFFECT-01:** ativação e parâmetros independentes por efeito, invalidação correta ao alternar, recursos liberados com segurança e testes das combinações com/sem animações.
- **ANIM-01:** implementar o motor em animation/, sem dependência de GPU ou materiais.
- **EFFECT-01:** implementar efeitos em effects/, começando pelo caminho simples e medindo cada passe; renderer/ executa a apresentação. Refração e reflexão dinâmica exigem medição antes de habilitar como padrão.

Referências: docs/visual-pipeline.md e docs/visual-style.md. O exemplo TOML v2 expressa essa divisão, mas continua sem parser ou aplicação ao vivo.

## Avaliação visual Glass V1

**DESIGN-01 · aguardando avaliação do usuário:** imagem conceitual em docs/design/jlinux-glass-v1.png, com página de prévia docs/design/README.md. Dock à esquerda, barra superior com Wi-Fi/Bluetooth/energia, menu de suspensão/reinício/desligamento e paleta roxo/rosa. Não é captura real ou funcionalidade implementada. Próxima ação: receber feedback e iterar a composição antes da interface nativa definitiva.

## Referências escolhidas pelo usuário — cf4238a

**DESIGN-01 · alvo visual definido pelo usuário:** as duas fotos do commit “Estilo que eu quero” foram localizadas e inspecionadas. Usar docs/design/README.md como índice. As propostas geradas V1/V2 foram rejeitadas e não orientam a próxima reprodução.

Próxima entrega visual: protótipo que reproduza fielmente composição, materiais, bordas, ícones, espaçamentos e fundo das referências. Comparar renderização e fotos na mesma resolução e registrar diferenças. Nenhum aplicativo, material ou integração foi implementado nesta atualização documental.

## Primeira entrega nativa — 0.5

- **NATIVE-01 · parcial implementado:** shell Wayland direto em C, sem GTK/CSS, com painel e dock à esquerda. Toolkit inicial (texto, superfícies, ícones, botões e hit testing) em src/toolkit/. Dock abre Foot e Codex por clique. Visual completo das referências e arquitetura final de materiais continuam pendentes.
- **TOOLKIT-01 · parcial:** Cairo/Pango/Fontconfig usados como bibliotecas de desenho/tipografia; cliente controla layout, entrada e buffers. Backend atual rasteriza SHM em CPU. GPU do toolkit, acessibilidade e escala precisam de trabalho/medição.
- **TEST-01 · parcial:** iniciador de sessão C, runtime privado e limpeza por eventos. Testes cobrem timeout/SIGTERM e ausência de runtime restante. Python ficou restrito a testes/ferramentas.
- **SHELL-01 · parcial:** Terminal/Codex funcionam; menus, dados de rede/Bluetooth e energia continuam sem integração e mostram aviso.
- **Próximo:** baseline PERF-01 e implementação progressiva do material em effects/ e renderer/, comparando com as referências. Não declarar Glass/fidelidade prontos por haver componentes nativos.

## Correção do material inicial

- **EFFECT-01 · parcial:** painel e dock amostram wallpaper com box blur em cache reduzido, tonalização e reflexos estáticos. Três controles independentes por ambiente; sem recarga ao vivo. Não desfoca janelas, não implementa refração/sombra difusa nem GPU Glass.
- **DESIGN-01 · em implementação:** prévia nativa em docs/design/native-glass-preview.png; wallpaper procedural provisório para tornar a amostragem visível. Continua diferente das fotos escolhidas; não representa aprovação do usuário.
- **Validação local:** seis testes release e ASan/UBSan passaram, incluindo impulso/constância do blur e oito combinações dos controles. Prévia inspecionada; não é captura DRM nem benchmark de GPU/energia.

## Ajuste visual de dock e barra — fix/glass-contrast

- Tonalização azul-acinzentada mais clara, reflexos localizados azuis/violetas, borda interna iluminada e botões menos opacos para tornar o material perceptível sobre fundo escuro.
- Wallpaper preservado. Escopo desta iteração: material do dock/barra; composição completa e ícones das referências continuam pendentes. Não declarar reprodução exata ou aprovação visual.
- Seis testes locais passaram em release e ASan/UBSan; prévia nativa atualizada. Sem medição de GPU/energia nesta alteração.

## Refinamento aprovado como direção — fix/glass-panel-spacing

- Barra reduzida de 54 para 44 px, margem de 12 para 6 px e reserva superior de 82 para 58 px (zona + margem). Texto, ícones, aviso e amostragem do fundo acompanham o novo layout; prévia usa as mesmas constantes.
- Bordas do dock/barra recebem bevel com iluminação direcional e contorno interno mais discreto, preservando o material mais claro da iteração anterior. Wallpaper preservado.
- Seis testes locais release e ASan/UBSan passaram; prévia inspecionada. Sem benchmark GPU/energia ou confirmação visual final do usuário.

## Dock compacto e contornos coloridos — fix/slim-dock-glow

- Dock de 68 para 56 px de largura e margem esquerda de 24 para 6 px; botões de 40×40 com ícones preservados. Raio, amostragem do fundo, área reservada e prévia acompanham as novas constantes.
- Reflexos localizados e contornos azuis/violetas mais brilhantes no dock e na barra. Wallpaper preservado.
- Seis testes release e ASan/UBSan passaram; clique no terminal atualizado para o centro do novo dock. Prévia inspecionada. Sem benchmark GPU/energia nesta alteração.

## Correção localizada — fix/panel-left-edge

Brilho do contorno externo na ponta esquerda da barra reduzido de 0,95 para 0,40 de alpha, retornando gradualmente ao brilho anterior após a curva. Dock, tonalização e restante do material preservados. Seis testes release passaram; prévia atualizada. Ajuste visual sem benchmark nesta alteração.

## Barra de 36 px e borda direita — fix/compact-panel-right-rim

- Barra de 44 para 36 px, preservando margem de 6 px. Reserva superior reduzida junto com a altura; conteúdo continua centralizado e prévia usa as constantes do shell.
- Borda e reflexo direitos da barra com azul-violeta menos rosado e intensidade mais uniforme. Dock e redução de brilho da ponta esquerda preservados.
- Seis testes release passaram; prévia inspecionada. Sem nova execução de sanitizers ou benchmark nesta alteração visual.

## Molduras próprias — feature/window-glass-frames

- **WM-GLASS · parcial implementado:** material compartilhado do shell nas molduras xdg-toplevel, cabeçalho de 36 px, título/fechar, negociação xdg-decoration server-side, foco por opacidade, arraste e resize pelas bordas esquerda/direita/inferior. Quatro faixas de buffer evitam imagem transparente do tamanho da janela; redesenho por geometria/título.
- **BAR-GLINT · implementado:** três pontos de reflexão estática junto às bordas da barra, controlados por REFLECTION, sem animação/poll adicional.
- **Validação local:** oito testes release e ASan/UBSan passaram, incluindo lifecycle das faixas e terminais reais com drag, fechar e resize confirmado por stty. Prévias dos componentes inspecionadas; não são screenshots de aplicativos nem aprovação visual do usuário.
- **Limites:** interior dos apps não retematizado, sem blur nas molduras, máscara arredondada do conteúdo, maximização/minimização ou benchmark GPU/energia. Aplicativos sem negociação de decoração podem manter seu cabeçalho interno.

## Cabeçalho retangular e cantos discretos — fix/rectangular-window-glass

- Moldura comum a todos os xdg-toplevel: cabeçalho retangular de 36 px, vidro compartilhado com shell, reflexos e contorno iluminado. Raio externo reduzido de 14 para 6 px; parte inferior do cabeçalho mantém junção reta com o conteúdo.
- Oito testes locais release e ASan/UBSan passaram; prévia do componente inspecionada e atualizada.
- Sessão principal ainda executa compositor antigo. Aplicar molduras nela exige reiniciar o compositor, encerrando clientes existentes; não realizado sem autorização específica para encerrar a sessão. Prévia aninhada anterior não equivale à aplicação global na sessão principal.

**Aplicação das molduras na sessão principal:** reinício autorizado pelo usuário e realizado. Logs DRM confirmam AMD Radeon, HDMI-A-1, Codex/painel/dock mapeados na versão nova. A pendência de aplicação na sessão principal foi resolvida; avaliação visual do usuário continua pendente.

## Maximizar/restaurar — feature/window-maximize

- **WM-MAX · implementado na branch:** botão no cabeçalho, Super+↑/Super+↓ e pedidos xdg de maximização, inclusive inicial. Restaura tamanho/posição, preserva moldura e respeita a área útil central. Arraste/resize manual bloqueado enquanto maximizada; foco e fechar continuam ativos. Minimizar/fullscreen seguem pendentes.
- **WORKAREA · correção:** recalcular área útil em map/unmap dos layers, além de commit/destroy. Reserva anterior permanecia integral na tela após map. Amostragem/prévia do dock acompanham o centro abaixo da barra.
- **Validação:** oito testes release e ASan/UBSan passaram; Foot real cobre maximizar/restaurar por botão/teclado, inicial maximizada, retorno do tamanho e resize posterior. Teste do shell confere geometria maximizada 1198×628 em 76,86 na saída simulada 1280×720, descontando barra/dock/moldura.
- **Ativação:** sessão principal preservada; nova versão do compositor ainda precisa ser iniciada. Sem benchmark ou validação física desta mudança.

**Feedback visual do usuário:** base inicial de vidro, cabeçalhos retangulares, bordas iluminadas e cantos discretos aprovada como versão inicial após o reinício anterior. Preservar essa direção; isso não conclui o Glass completo.

**WM-MAX · ativação realizada:** usuário autorizou reinício e a versão nova foi iniciada na sessão DRM principal, com Codex, painel e dock mapeados. Pendente avaliação física pelo usuário; testes automatizados já registrados acima.

## Vidro mais evidente nas janelas — fix/window-glass-depth

- Cabeçalho com reflexão em camadas e pontos de luz no topo/laterais, contorno interno fino para destacar espessura. Efeito exclusivo das molduras em effects/, sem alterar dock/barra, geometria ou hit testing. REFLECTION desativado remove o acabamento adicional; sem animação ou timer novo.
- Oito testes release e ASan/UBSan passaram; teste do componente compara transparência do brilho com/sem reflexão. Prévia inspecionada e atualizada. Sem blur/refração nas molduras ou benchmark GPU/energia.
- Sessão DRM reiniciada usando autorização do usuário, com Codex/painel/dock mapeados. Pendente avaliação visual do usuário.

## Bordas de janela com acabamento do dock — fix/window-dock-material

- Gradiente de contorno extraído em helper compartilhado: molduras reutilizam a paleta do dock. Clarões pontuais extras nas laterais removidos; faixa suave contínua e linha interna reforçam espessura. Geometria, controles e material do dock preservados.
- Oito testes release e ASan/UBSan passaram; prévia do componente atualizada/inspecionada. Sessão DRM reiniciada conforme autorização vigente; Codex/painel/dock mapeados. Aprovação visual pendente; sem benchmark ou blur nas molduras.

## Bordas finas com pontos intensos — fix/thin-window-highlights

- Iteração anterior rejeitada pelo usuário por espessura excessiva. Moldura lateral/inferior reduzida de 6 para 3 px; faixa larga de brilho removida. Contorno discreto de 0,8 px e quatro reflexos localizados com núcleo branco intenso. Cabeçalho retangular de 36 px e raio de 6 px preservados; dock/barra intactos.
- Oito testes release e ASan/UBSan passaram. Geometria maximizada atualizada nos testes; contraste entre ponto brilhante e trecho discreto verificado no buffer. Prévia inspecionada. Sessão DRM reiniciada conforme autorização vigente, com Codex/painel/dock mapeados. Aprovação visual e benchmark pendentes.

## Sombra interna e contorno superior fino — fix/window-rim-shading

- Sombra interna leve, independente de REFLECTION, destaca bordas de janela sem alterar os 3 px aprovados. `JLINUX_GLASS_SHADOW=0/1` controla apenas esse sombreado, sem drop shadow externo ou blur.
- Barra superior e cabeçalhos compactos com contorno de 0,75 px, bevel reduzido e sem segunda linha interna. Altura de 36 px e brilhos preservados; dock vertical inalterado.
- Oito testes locais release e ASan/UBSan passaram; teste verifica sombreamento com reflexão desligada e rejeição de parâmetro inválido. Prévias inspecionadas. Aplicação à sessão corrente ainda pendente nesta etapa; próxima entrega instala sessão do Arch solicitada pelo usuário.

## Instalação Arch e login próprio — feature/arch-session-install

- **ARCH-SESSION · em validação:** pacote pacman, entrada Wayland, configuração greetd com backup e TTY2 de recuperação. Usuário autorizou instalar/iniciar a interface no boot; não há autorização para merge/release ou reboot automático do computador.
- **LOGIN-01 · implementado em validação:** greeter C/Wayland com material nativo, usuário/PAM, campos secretos mascarados, compose XKB e IPC JSON limitado/não bloqueante. greetd autentica; Cage isola a tela de login. Desktop mantém compositor próprio. Prévia explicitamente não autentica.
- **RECOVERY-VT · implementado:** compositor DRM recebe sessão wlroots emprestada e encaminha Ctrl+Alt+F1…F12; não cria/destrói a sessão do backend. Teste físico pendente.
- **TESTES:** dez testes release e ASan/UBSan passaram antes do empacotamento; IPC usa servidor simulado, sem senha real. Instalação/boot e PAM físico ainda pendentes nesta etapa. Instruções em docs/arch-installation.md.

### Instalação concluída e boot habilitado

- **ARCH-SESSION · instalado/configurado:** pacote pacman `0.5.0.r33.g45c6752-1` compilado com flags Arch e dez testes aprovados. Binários/root ownership, entrada de sessão e instalador conferidos. Sessão DRM principal agora executa `/usr/bin/jlinux-session`, compositor e shell do pacote. Últimos ajustes de sombra interna/contornos superiores finos estão ativos.
- **BOOT-LOGIN · configurado, validação física pendente:** greetd habilitado como display-manager para o próximo boot, alvo graphical, sem autologin. TTY2 habilitado/ativo e backup anterior salvo. Nenhum reboot do computador ou substituição do login TTY1 ativo foi feito.
- **LOGIN-TEST · onze testes locais release/ASan/UBSan aprovados:** inclui fluxo completo de digitação no greeter com greetd simulado, além de prompts/recusa/cancelamento/limites. O login PAM real não foi simulado como resultado físico; aguarda usuário no próximo boot. PR permanece draft até essa confirmação.

**RECOVERY-VT · correção validada no hardware:** troca real por chvt revelou remoção temporária do HDMI ao pausar DRM. Compositor passa a aguardar/reassociar saída e mantém layers em vez de encerrar. Teste de ida/volta preservou processos e confirmou modeset de retorno; combinação física de teclas e login PAM ainda dependem do usuário. Script manual incluído e onze testes release/ASan/UBSan aprovados.

## Primeiro boot real e relato de travamento

- **BOOT-LOGIN · confirmado no boot de 05/11/2026 16:40:** greetd ativo, PAM abriu sessão do usuário jlinux às 16:40:27; supervisor/compositor/shell executam `/usr/bin`, pacote `0.5.0.r37.g77ecbc1-1`, com terminal/Codex em uso. Não marcar mais primeiro login como pendente.
- **STABILITY-BOOT · P0 · investigando:** usuário relatou congelamento seguido de reinício/desligamento não limpo. Journal anterior termina após testes de VT; não há evidência suficiente para atribuir a causa a GPU/kernel/compositor. Sessão atual viva, memória disponível, nenhum novo reset GPU/OOM identificado. Evitar novos testes de troca de VT na sessão de trabalho enquanto se investiga.
- **RECOVERY-VT · evidência corrigida:** três ciclos passaram na build local com terminal; a execução final do mesmo teste no pacote foi interrompida pelo reboot e seu resultado não foi recuperado. Não afirmar que esse teste final de pacote foi concluído.
- **EFI-DIRTY · pendente de verificação:** kernel registrou EFI FAT não desmontada corretamente após reinício abrupto. Nenhum reparo ou alteração de partição executado.

## Remoção da marca na barra — fix/remove-panel-brand

Nome e símbolo JLinux removidos da barra superior; menus começam a 24 px da margem interna. Layout restante preservado. Build e teste shell-headless passaram; prévia atualizada. Empacotamento aponta para esta branch para instalar a revisão correta. Ativação visível depende de novo shell; sessão greetd atual preservada.

**Remoção da marca · instalada:** pacote `0.5.0.r39.g766bcaf-1` instalado pelo pacman, com onze testes do pacote aprovados. Sessão atual do greetd preservada, sem logout/reinício ou teste de VT. Nome/símbolo removidos ficarão visíveis no próximo login; processo antigo ainda desenha a barra atual.

## Locale do terminal — fix/terminal-locale

- Logs confirmaram Foot iniciando sem LANG, caindo em C e avisando sobre UTF-8, embora /etc/locale.conf configure pt_BR.UTF-8. Iniciador C passa a carregar LANG do arquivo do sistema quando ausente, com fallback C.UTF-8, sem avaliar comandos e sem sobrescrever LANG/LC_* explícitos.
- Doze testes release e ASan/UBSan passaram: locale válido/entre aspas, ambiente explícito preservado, entradas inválidas ignoradas, fallback e terminal real sem aviso de locale em ambiente limpo.
- Avisos de protocolos Wayland ainda não implementados permanecem nos logs. A mensagem exata mencionada pelo usuário ainda não foi confirmada; não tratar a correção de locale como resolução de todo possível problema do terminal.

**TERMINAL-UTF8 · instalado:** pacote `0.5.0.r41.g94ddb52-1` instalado pelo pacman após doze testes de pacote aprovados. Terminal adicional aberto na sessão atual com LANG=pt_BR.UTF-8 e ausência do aviso de locale confirmada em log próprio. Sessão atual preservada. Correção herdada automaticamente por dock/atalho/terminal inicial requer próximo login, pois os processos antigos conservam ambiente anterior. Outros avisos Wayland permanecem como diagnósticos.

## Gerenciador de arquivos — feature/file-manager

- FILES-01 implementado: aplicativo C/Wayland separado, toolkit próprio, barra Glass, pastas primeiro, seleção/rolagem, navegação, ocultos e abertura pelo aplicativo associado via GIO sem shell. Primeiro ícone do dock e entrada desktop integrados. Sem operações de escrita de arquivos nesta etapa.
- Quatorze testes locais release e ASan/UBSan passaram. Novos testes de modelo e UI headless cobrem nomes especiais/UTF-8, links, erro de navegação, teclado, clique e argumentos de abertura; teste do shell verifica lançamento pelo dock. Prévia com dados fictícios inspecionada. Sem benchmark ou teste de VT.
- Empacotamento/ativação ainda pendentes neste registro. Novo dock exige próximo login; aplicativo pode abrir na sessão existente. Limites em docs/files.md.

**FILES-01 · instalação:** pacote `0.5.0.r43.gf98f87e-1`, fonte f98f87e, compilado do GitHub com 14 testes aprovados no makepkg e instalado pelo pacman. `/usr/bin/jlinux-files` aberto na sessão autenticada existente e permaneceu ativo após inicialização. Shell/compositor existentes preservados, sem logout, reinício ou troca de VT. Novo dock só no próximo login. Inspeção visual física pelo usuário pendente; CI ainda em execução neste registro. PR #30, base temporária fix/terminal-locale, sem merge.

## Bordas neon discretas — feature/window-neon-rim

Pedido atual substitui pontos focais nas janelas: contorno contínuo de 0,85 px azul/violeta, halo interno discreto de 2,8 px recortado à moldura, sem núcleos brancos. Cabeçalho retangular de 36 px, borda física de 3 px e cantos de 6 px preservados. Dock/barra superior não mudam. Gerenciador recebe fundo opaco em gradiente azul-ardósia mais claro e barra interna com o mesmo acabamento contínuo; não altera conteúdo de aplicativos externos.

Quatorze testes locais release e ASan/UBSan passaram. Teste da moldura passa a conferir continuidade de opacidade e variação cromática em vez dos antigos pontos intensos; controles independentes preservados. Prévias dos componentes inspecionadas junto às referências, sem declarar correspondência exata. Sem timer, animação, blur novo ou benchmark. Ativação das molduras exige novo compositor no próximo login; sessão atual preservada devido à investigação STABILITY-BOOT.

**NEON-01 · instalado:** pacote `0.5.0.r45.g13cadc7-1`, fonte 13cadc7, compilado do GitHub com 14 testes aprovados no makepkg e instalado via pacman. Apenas o gerenciador foi reaberto com o novo fundo e barra interna; processo permaneceu ativo. Compositor/shell da sessão preservados: novas molduras globais aguardam próximo login. Sem teste físico de VT, benchmark ou confirmação visual pelo usuário. PR #31 sem merge; CI pendente neste registro.

## Sidebar do Arquivos — feature/files-sidebar

FILES-02 implementado: barra lateral Pessoal/Downloads/Documentos/Imagens/Vídeos com caminhos XDG, destaque da pasta atual, clique e Alt+1…5. Pastas ausentes não são criadas; erro preserva o diretório anterior. Lista reposicionada e coluna de detalhes oculta em janela estreita, mínimo 540×360.

Quatorze testes locais release e ASan/UBSan passaram. Teste headless ampliado abre os cinco locais com HOME/configuração isolados, nomes personalizados com espaços/acentos, verifica argumentos do aplicativo associado e tentativa de pasta ausente. Prévia com arquivos fictícios inspecionada. Sem benchmark, teste de VT ou confirmação visual física. Empacotamento/ativação pendentes neste registro. Alteração local concorrente em src/shell/main.c preservada fora desta entrega.

**FILES-02 · instalado:** pacote `0.5.0.r47.g0578b80-1`, fonte 0578b80, compilado do GitHub com 14 testes aprovados no makepkg e instalado via pacman. Gerenciador instalado reaberto com sidebar na sessão existente e permaneceu ativo. Esta entrega não reiniciou compositor/shell nem realizou troca de VT. PR #33, base temporária feature/window-neon-rim, sem merge. CI e avaliação visual pelo usuário pendentes neste registro. Alterações concorrentes de shell/empacotamento preservadas fora dos commits desta entrega.


## Energia na barra superior — feature/power-menu

- **SHELL-POWER · implementado:** botão de energia abre Sair da sessão, Reiniciar e Desligar, com confirmação e cancelamento. Logout direciona XDG_SESSION_ID validado; reiniciar/desligar usam systemctl sem solicitar senha e respeitando inibidores. Erros de execução/recusa são mostrados no menu e registrados. Sem sudo, força ou mudanças PAM/PolicyKit.
- Menu C/Wayland separado em layer overlay, sem alterar a área útil de janelas. Modo transitório --power-panel disponibiliza apenas o botão e menu sobre a sessão antiga; shell normal já inclui as ações no próximo login. Demais funcionalidades da branch top-panel (#29) não fazem parte desta branch.
- Prévia nativa do componente inspecionada. Teste headless usa PATH restrito a loginctl/systemctl simulados: verifica argumentos das três ações, confirmação, cancelamento e recusa, tanto no shell completo quanto no modo transitório. Nenhuma ação real de logout/reinício/desligamento foi executada durante o desenvolvimento.
- Build release e 15 testes locais passaram; 15 testes ASan/UBSan também passaram. Instalação registrada abaixo após conclusão. Consulta read-only ao logind retornou CanPowerOff=yes e CanReboot=yes nesta máquina. Isso não substitui validação física das ações pelo usuário.


**SHELL-POWER · instalado/ativado:** pacote `0.5.0.r50.g023034a-1`, fonte 023034a, com 15 testes de pacote aprovados, instalado pelo pacman. Botão transitório C/Wayland mapeado na sessão greetd ativa, sem encerrar o shell supervisionado, compositor ou aplicativos. Próximo login usa o menu integrado. Nenhuma ação real de energia/logout nem teste VT executado; confirmação física pelo usuário pendente. PR #34 para feature/files-sidebar (#33), sem merge. Log de ativação em docs/arch-installation.md.


## Tons dark e fonte menor — feature/darker-desktop

- **DARK-01 · implementado:** textos do toolkit a 93% do tamanho nominal; tint azul-preto mais escuro e reflexo interior reduzido; contornos compartilhados azul/violeta mais saturados/intensos sem engrossar. Fundo de Arquivos escurecido. Sem alteração de wallpaper, ícones, dimensões ou preferências independentes dos efeitos.
- Alcance: shell/menu de energia, Arquivos, molduras e greeter que compartilham toolkit/material. Não altera fontes/temas de apps externos nem inclui o navegador em branch separada.
- Validação local: build release, 15 testes aprovados, diff --check; prévias de Arquivos antes/depois em 1000×640 e de shell/moldura inspecionadas. Sem nova execução de sanitizers, benchmark ou teste VT nesta mudança de constantes visuais.
- Branch dependente de feature/power-menu (#34); trabalho local concorrente de compositor preservado. Instalação e ativação registradas abaixo após conclusão. Shell/compositor antigos não têm recarga visual; aplicação integral exige próximo login.


**DARK-01 · instalado:** pacote `0.5.0.r52.gbb964fe-1`, fonte bb964fe, compilado do GitHub com 15 testes aprovados no makepkg e instalado via pacman. Nova janela de Arquivos instalada aberta e permaneceu ativa (PID 54544 neste boot). Shell/compositor da sessão greetd e aplicativos existentes preservados; fonte/material globais da barra e molduras entram no próximo login. Menu de energia transitório anterior continua ativo. Sem logout/reinício/VT; CI e avaliação visual física pelo usuário pendentes neste registro. PR #37, base feature/power-menu (#34), sem merge.

## Dock configurável — feature/configurable-dock

DOCK-01 implementado: lista nativa de aplicativos .desktop via GIO, botão +, fixar/remover com persistência atômica por usuário e scroll no dock/lista. Três ações básicas e geometria preservadas. Atalhos adicionais usam monogramas, não ícones reais. Configuração inválida preservada; limite 32; sem comando de shell montado para lançar aplicativos. Modo auxiliar --dock-panel permite ativação sem encerrar a sessão, com fundo próprio opaco; não confundir com material integrado ou recarga do processo antigo.

Testes locais release e ASan/UBSan, prévia e instalação registrados na conclusão abaixo. Fluxo isolado cobre pin, launch, sinais do filho, persistência, scroll, unpin e configuração inválida em ambos os modos. Sem benchmark, teste de VT ou validação de multimonitor/acessibilidade completa. Limites em docs/dock.md. Base na branch do tema escuro para preservar o visual instalado; alterações concorrentes da árvore principal não incluídas.

**DOCK-01 · concluído/instalado:** 17 testes locais release e ASan/UBSan passaram. Pacote `0.5.0.r54.ge7ec423-1`, fonte e7ec423 da branch feature/configurable-dock, compilado do GitHub com 17 testes aprovados e instalado via pacman, preservando a base do tema escuro. `/usr/bin/jlinux-shell --dock-panel` ativado na sessão existente e permaneceu ativo; ambiente herdou LANG=pt_BR.UTF-8. Nenhum aplicativo foi fixado por conta do usuário. Supervisor/compositor/shell principal não foram encerrados; sem VT, logout ou reinício nesta entrega. Runtime principal permanece no código anterior, com novo dock em processo auxiliar; próximo login usa integração do pacote. Prévia dos componentes inspecionada; confirmação visual física e CI pendentes neste registro. PR #39, base feature/darker-desktop, sem merge.


## Controle simples de volume — feature/panel-volume

- **SHELL-VOLUME · implementado:** botão Som na barra superior abre volume da saída padrão, ±5% com limite de 100% e mute/reativar. Consulta ao abrir, após ajuste, por clique no estado e a cada 30s com menu aberto. Processos wpctl sem shell, um por vez, timeout monotônico de 3s; estado indisponível em falha. Sem seleção de dispositivos, microfone, volume por app ou navegação por teclado.
- Backend C separado, ambiente PipeWire do usuário preservando override explícito; não instala/ativa serviços nem altera preferências do áudio. Modo transitório --volume-panel permite botão funcional sem encerrar o shell supervisionado. Prévia do componente inspecionada, 40% são fixture visual.
- Testes: respostas válidas/mute/acima de 100% e recusa de NaN/infinito/negativo/lixo; cliques headless com wpctl simulado verificam argumentos, nova consulta após ajuste, erro, timeout e recuperação no shell completo e modo transitório. Build release e 19 testes aprovados; 19 testes ASan/UBSan também passaram. Instalação registrada após conclusão. Sem alteração de volume real durante os testes de cliques, benchmark ou VT.
- Base atual feature/window-auto-layout (#42), incluindo o dock de fix/visible-browser-dock (#41) e o layout automático instalados durante esta entrega. Sem merge de PR.
## Organização automática — feature/automatic-tiling

- **WM-TILE · implementado na branch:** principal + pilha, ordem estável independente do foco, divisão ajustável, flutuante por janela e modo manual por variável de ambiente. Cálculo por eventos, três passagens O(n), sem alocações no rearranjo; geometrias iguais não são reenviadas.
- **WM-03 · parcial:** nove espaços, troca/envio e recuperação de foco; diálogos xdg-parent acompanham o envio do pai. Regras persistentes, configuração e indicador no shell continuam pendentes.
- Tela cheia xdg ganha árvore acima dos layers top e abaixo dos overlays, sem molduras; maximização continua separada e restaura para o layout.
- Validação local: 16 testes release e 16 ASan/UBSan em headless/Pixman; `git diff --check`. Evidência e limites: `docs/window-management.md`. Testes headless com clientes reais não comprovam velocidade em GPU nem equivalência visual. PERF-01 continua pendente; há plano de comparação manual/automático no documento.
- Instalação ativa preservada: pacote observado `jlinux-desktop-git 0.5.0.r50.g023034a-1`; esta branch não foi instalada e a sessão greetd não foi reiniciada. Nenhum teste de VT/DRM.

## Ajuste automático na base atual — feature/window-auto-layout

WM-AUTO integrado a partir de 6a0f028 sobre a base instalada 8c5c803: uma janela preenche a área útil, duas dividem a tela, demais empilham à direita; abrir/fechar e mudar área útil reorganizam por eventos. Mantidos atalhos de proporção/principal/flutuante, workspaces, limites e diálogos. Dock configurável e tema preservados. Base temporária fix/visible-browser-dock; reaproveitamento de #38, sem integrar PRs.

Dezenove testes release passaram nesta base. ASan/UBSan e empacotamento registrados na conclusão abaixo. Compositor atual preservado; ativação somente no próximo login. Sem reinício, VT, benchmark ou declaração de validação física.

**WM-AUTO · instalado em 06/11/2026:** pacote `0.5.0.r58.gc1bcc75-1`, fonte c1bcc75 (implementação integrada em 0e31436), compilado do GitHub com 19 testes aprovados no makepkg e instalado pelo pacman. Dezenove testes locais release e ASan/UBSan passaram. PR #42, base fix/visible-browser-dock, reutilizando implementação de #38 sem merge de PR. Compositor da sessão atual preservado e ainda executa código anterior: comportamento automático será ativado no próximo login, por padrão, salvo JLINUX_TILING=0 explícito. Não houve logout, reinício, VT ou teste físico de GPU nesta entrega. CI e avaliação visual do usuário pendentes no registro.


**SHELL-VOLUME · instalado/ativo:** pacote `0.5.0.r61.g841643c-1`, fonte 841643c, com 21 testes de pacote aprovados. Integra o layout automático de #42 para preservar a instalação concorrente c1bcc75. Cliente transitório instalado mapeou `jlinux-volume-button` na sessão greetd 3 do boot atual (PID 23834; shell principal 719 preservado). Log: `/tmp/jlinux-volume-live-kfk36qww/volume.log`. Backend C consultou a saída real a 40%, sem mute, simulando runtime Wayland privado; nenhuma alteração de volume ou reprodução de som real foi feita. Shell normal inclui o botão no próximo login; compositor/dock/aplicativos ativos preservados. PR #43, base temporária feature/window-auto-layout, CI parcialmente concluída no registro. Validação auditiva física pelo usuário pendente.
