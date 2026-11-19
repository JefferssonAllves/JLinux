# animation/

Base implementada em transition.c: interpolação escalar smoothstep com relógio fornecido pelo chamador, retarget sem salto e duração zero para movimento reduzido. Usada inicialmente pelo dock; ainda não é um motor geral de animações de janelas.

- Entrada: instante monotônico, duração, curva, valores inicial/final e política de movimento reduzido.
- Saída: propriedades atualizadas e indicação de que outra amostra será necessária.
- Sem dependência de GPU, wlroots, shader, blur ou material Glass. Animações devem ser testáveis com relógio controlado.
- A mesma avaliação poderá mover janelas e variar parâmetros de um efeito habilitado. O efeito não mantém relógio ou loop próprio.
- Ao desativar animações, aplicar o estado final; efeitos estáticos podem continuar habilitados.

Contrato compartilhado e regras de ativação: [docs/visual-pipeline.md](../../docs/visual-pipeline.md).
