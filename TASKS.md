# Plano central do JLinux

Este arquivo é a fonte central das tarefas iniciais. Personalização ampla, compositor próprio, shell separado e eficiência mensurada orientam todas as entregas. Responsável pela execução: coordenador Codex; perfis abaixo indicam especialidade, não execução paralela automática.

Estados: **concluído** = implementação e evidências indicadas; **em andamento** = trabalho ativo; **pendente** = não entregue. Uma tarefa pode estar concluída na branch sem estar integrada em develop/main. Cada entrega registra PR, validação e limites. CI verde não é teste de GPU.

## Preparação e primeira interface

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| BASE-01 | P0 · concluído | Inventariar hardware e decidir base | — | coordenador | docs/hardware.md e docs/architecture.md; capacidades e incertezas explícitas |
| BASE-02 | P0 · concluído | Núcleo Wayland próprio | BASE-01 | compositor-engineer | Compila; saída e socket headless; SIGTERM limpa recursos; smoke test passa |
| DEV-01 | P0 · pendente de integração | Formalizar branches e PRs | BASE-02 | coordenador | PR #1 validado e integrado por autorização; sem push direto em branches duradouras |
| DEV-02 | P0 · em andamento | Configurar perfis, skills e ferramentas | DEV-01 para integração | coordenador | TOML/skills validados, fontes registradas e PR próprio; plugins sem conexão identificados |
| UI-01 | P0 · em andamento | Desktop visual mínimo | BASE-02 | visual-engineer + compositor-engineer | Fundo e barra JLinux com relógio; shell em processo separado; execução reproduzível e teste de mapeamento de superfície |
| TEST-01 | P0 · pendente | Isolar execução e instalação | BASE-02 | correctness-reviewer | Runtime temporário e cleanup; prefixo de desenvolvimento; documentar execução aninhada e recuperação sem substituir sessão cotidiana |
| PERF-01 | P0 · pendente | Linha de base de desempenho | UI-01, TEST-01 | performance-reviewer | Registrar commit/backend, PSS, CPU ociosa e workload; distinguir medição headless de GPU; salvar metodologia e resultados reproduzíveis |

## Tornar o desktop interativo

| ID | Prioridade / estado | Tarefa | Dependências | Perfil | Critério de conclusão |
| --- | --- | --- | --- | --- | --- |
| WM-01 | P0 · pendente | xdg-shell e janelas | BASE-02 | compositor-engineer | Configure/map/unmap/destroy e popups; múltiplos clientes reais; desconexão sem crash |
| INPUT-01 | P0 · pendente | Teclado e ponteiro | WM-01 | compositor-engineer | Seat, keymap, movimento, cliques, foco e hotplug; não perder entrada após fechar janela |
| WM-02 | P1 · pendente | Mover, redimensionar e atalhos | WM-01, INPUT-01 | compositor-engineer | Interações com validação de serial; fechar janela, alternar foco e encerrar sessão |
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

**UI-01:** fornecer a interface mais básica possível e evoluir conforme o feedback do usuário. Não antecipar efeitos caros, toolkit completo ou adaptação automática antes de haver base interativa e medições. Atualizar este arquivo no PR da interface com evidências e limitações reais.
