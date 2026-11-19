---
name: jlinux-performance
description: Medir ou investigar consumo, latência e fluidez do JLinux, comparar otimizações e planejar adaptação de efeitos ao hardware. Não usar como rotina obrigatória para mudanças sem impacto em execução.
---

# jlinux-performance

Defina primeiro a hipótese, workload, baseline, métrica e backend. Registre commit, versões, renderizador, resolução/frequência, alimentação e condições relevantes. Faça medições limitadas e reproduzíveis; mantenha dados brutos fora de commits quando contiverem identificadores locais.

Use PSS para memória compartilhada; observe CPU em repouso, despertares e percentis de tempo de quadro. Diferencie tempo de renderização, apresentação e latência ponta a ponta. Não compare build com sanitizers a release como evidência de otimização.

Headless/Pixman serve a testes determinísticos; ganhos de GPU, energia e direct scanout exigem hardware real. Não invente leituras se o sensor não existe. Apresente incerteza e repetições suficientes para a conclusão.

A adaptação deve ter histerese, limites explícitos e qualidade efetiva observável. Não mude resolução/frequência nem desative preferências silenciosamente. Entregue resultado mensurado ou plano de medição, distinguindo ambos.
