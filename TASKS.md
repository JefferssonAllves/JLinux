# Plano central do JLinux

Este arquivo é a fonte central das tarefas iniciais. Personalização ampla, compositor próprio, shell separado e eficiência mensurada orientam todas as entregas. Responsável pela execução: coordenador Codex; perfis abaixo indicam especialidade, não execução paralela automática.

Estados: **concluído** = implementação e evidências indicadas; **em andamento** = trabalho ativo; **pendente** = não entregue. Uma tarefa pode estar concluída na branch sem estar integrada em develop/main. Cada entrega registra PR, validação e limites. CI verde não é teste de GPU.

## Diretriz para as próximas telas

As ferramentas web servem para criar e iterar protótipos visuais antes da interface definitiva nativa. O shell GTK/CSS atual é provisório. Tecnologias de produção serão escolhidas por adequação e desempenho medido, preservando personalização.

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
