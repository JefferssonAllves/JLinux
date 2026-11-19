---
name: jlinux-visual
description: Criar ou modificar o shell e a personalização visual do JLinux, incluindo temas, cores, layout, componentes e animações. Não usar para sites ou aplicações web sem relação com o desktop.
---

# jlinux-visual

Leia docs/architecture.md e o estado real de src/ antes de escolher a abordagem. Preserve shell em processo separado e a possibilidade de trocar temas sem reimplementar políticas de janelas.

Comece com componentes pequenos. Exponha tokens semânticos de cor, espaçamento e tipografia; ao criar parser, valide configuração completa antes de aplicar e preserve o último estado válido. Documente quando o arquivo for apenas exemplo.

Animações usam tempo monotônico, aceitam interrupção e movimento reduzido. Evite desenhar componentes invisíveis ou animar continuamente uma cena parada. Reutilize bibliotecas de tipografia; toolkit próprio não implica escrever shaping de texto do zero.

Valide em sessão aninhada ou headless conforme a tarefa. Captura de tela é evidência visual, não de foco/entrada ou desempenho. Registre limites do protótipo e próximos passos em TASKS.md.
