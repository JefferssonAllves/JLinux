# Plano central do JLinux

Este arquivo é a fonte central das tarefas iniciais. Personalização ampla, compositor próprio, shell separado e eficiência mensurada orientam todas as entregas. Responsável pela execução: coordenador Codex; perfis abaixo indicam especialidade, não execução paralela automática.

Estados: **concluído** = implementação e evidências indicadas; **em andamento** = trabalho ativo; **pendente** = não entregue. Uma tarefa pode estar concluída na branch sem estar integrada em develop/main. Cada entrega registra PR, validação e limites. CI verde não é teste de GPU.

## Diretriz para as próximas telas

As ferramentas web servem para criar e iterar protótipos visuais antes da interface definitiva nativa. O shell GTK/CSS foi substituído pela primeira base Wayland/C; o toolkit ainda é inicial. Tecnologias de produção serão escolhidas por adequação e desempenho medido, preservando personalização.

## Navegador próprio — feature/web-browser

### Conteúdo abaixo do cabeçalho — fix/browser-content-layout

- **BROWSER-LAYOUT · implementado na branch:** cabeçalho em área própria acima da origem da página; viewport recebe altura da janela menos 148 px, inclusive após trocar abas ou redimensionar. Cliques de cabeçalho e conteúdo tratados nas respectivas superfícies, preservando renderização WPE.
- **Regressão local:** mede o viewport e testa topo/rodapé clicáveis, sete abas sem tamanho intermediário incorreto, entrada no cabeçalho maximizado, maximizar/restaurar e resize pela borda. Passaram os 16 testes locais release e os quatro do navegador com ASan/UBSan (`detect_leaks=0`). CI e pacote serão registrados após envio.
- **Limites:** sem novo benchmark ou validação visual em GPU/HiDPI; não modifica CSS de sites, sessão de login ou desktop instalado.

### Barra persistente e Google — feature/browser-search-bar

- **BROWSER-SEARCH · implementado:** termos abrem Google com parâmetros codificados; URLs continuam validadas. Barra/abas permanecem visíveis, com foco de edição separado; Esc cancela e devolve o foco à página. URI atualiza fora da edição.
- **Checks locais:** 16 testes release, incluindo teclado/mouse sem reabrir barra, primeiro clique na página, buscas/entradas inválidas e regressões de mídia/abas. Quatro testes do navegador com ASan/UBSan passaram (`detect_leaks=0`). Oito checks do pacote passaram; instalado `0.1.0.r46.g6e7897c-1`, código `6e7897c`, em 06/11/2026. Busca Google via executável instalado completou DOM/JS. PR #46 enviado, CI em execução; janelas existentes preservadas até reabrir.
- **Correção posterior:** a sobreposição desta entrega foi removida em `fix/browser-content-layout`, acima. Sem mudança de CSS dos sites ou novo benchmark.

### Mídia, estabilidade e instalação — feature/browser-media

- **BROWSER-MEDIA · validado localmente:** MP4 H.264/AAC, WebM VP8/Opus e MediaSource com MP4 fragmentado; teste verifica tempo avançando e frames decodificados em Wayland headless. Plugins base/good/libav declarados no novo pacote separado. Autoplay com áudio continua exigindo interação; sem Widevine, fullscreen do desktop ou alegação de compatibilidade universal.
- **BROWSER-CRASH · causa identificada/corrigida:** dois cores WPE 2.54.2 abortam em `createAudioSink`; faltava `gst-plugins-good`/autoaudiosink. Plugins instalados e pré-verificação adicionada: mídia/WebAudio desativados com aviso se o sink/player faltarem. Falha simulada do processo web não fecha o aplicativo; Ctrl+R recupera a página e o vídeo. Evidências em docs/browser.md.
- **BROWSER-INSTALL · em validação:** receita `packaging/arch/browser/PKGBUILD` instala somente o navegador e `.desktop`, sem substituir o desktop atual. Worktree persistente em `~/.local/src/jlinux-browser`; a antiga em `/tmp` foi apagada no reboot. Fixação no dock do usuário usa a configuração existente, sem novo shell/compositor paralelo.
- **Checks locais:** 16 testes release; quatro testes do navegador com ASan/UBSan e `detect_leaks=0`. LeakSanitizer, hardware visual/áudio, CI e benchmark separados. PipeWire/Pulse/WirePlumber instalados e iniciados para o áudio do aplicativo; saídas analógica/HDMI detectadas, sem alterar volume/saída ou reiniciar sessão. Pacote/ativação final serão registrados após instalação.

### Abas — feature/browser-tabs

- **BROWSER-TABS · implementado na branch:** Ctrl+T/“+” abre aba; Ctrl+Tab/Ctrl+Shift+Tab, títulos clicáveis e setas alternam; Ctrl+W/“×” fecha, inclusive a última (encerra a janela). Um toplevel, WebView/histórico/DOM por aba, sessão efêmera compartilhada e somente a view ativa visível. Fileira integrada à barra provisória aberta por Ctrl+L; títulos e avisos associados à aba correta. Sem dependências novas ou mudanças no desktop.
- **Validação local:** teste headless/Pixman com sete abas, teclado/mouse, preservação de formulário sem recarregar, histórico próprio, fechamento de ativa/inativa/última, overflow da fileira e fechamento xdg com múltiplas abas. Suíte release e ASan/UBSan registrados em docs/browser.md; LeakSanitizer permanece pendente como na base. Prévia do componente atualizada/inspecionada, sem aprovação visual definitiva ou benchmark novo.
- **Dependência:** PR de abas parte de `feature/web-browser`/PR #32, ainda aberto. As pendências de downloads, persistência, restauração/suspensão de abas, IME/acessibilidade, GPU/energia e comparação de motores não fazem parte desta entrega.

### Histórico da primeira entrega

- **BROWSER-01 · parcial implementado na branch:** aplicativo C/WPE WebKit 2.54 separado do desktop, build opt-in, uma página por janela, endereço nativo sobreposto por Ctrl+L, navegação/reload/histórico e perfil efêmero. Material compartilhado; ainda é interface provisória, sem abas ou reprodução completa das referências.
- **BROWSER-TEST · validação local:** 15 testes release passaram. Teste Wayland/Pixman cobre texto em página, endereço nativo, reload, voltar/avançar, maximização, Ctrl+Q e fechamento xdg. HTTP local/DOM/JavaScript/redirecionamento, bloqueio de esquema local e recusa de TLS autoassinado testados. HTTPS externo em example.org passou. ASan/UBSan nos três testes do navegador passaram com `detect_leaks=0`; LeakSanitizer permanece pendente por alocações com stacks em bibliotecas externas.
- **BROWSER-PERF · baseline inicial, sem comparação:** três processos headless novos, página mínima local, 199–218 ms até DOM/JS prontos e pico de PSS amostrado da árvore de 229–233 MiB. Isso não demonstra consumo em repouso, desempenho em GPU ou superioridade ao Firefox/Chromium. Dados e metodologia em [docs/browser.md](docs/browser.md).
- **BROWSER-02 · pendente:** validar composição/entrada/compatibilidade no caminho acelerado aninhado, medir repouso/vídeo/sites reais contra Firefox/Chromium e decidir o motor por evidência. Depois implementar layout definitivo validado, abas, downloads, persistência e permissões. Acessibilidade/IME da barra também pendentes.
- Branch derivada de `fix/terminal-locale` (PR #28 aberto), conforme fluxo de PR dependente; alterações locais de `feature/file-manager` preservadas em outra worktree. Instalação e sessão greetd preservadas; nenhum teste de VT/DRM realizado.

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
