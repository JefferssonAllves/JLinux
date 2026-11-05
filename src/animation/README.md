# animation/

Responsabilidade planejada: avaliar tempo, curvas e transições de propriedades. O motor ainda não foi implementado; este diretório define sua fronteira.

- Entrada: instante monotônico, duração, curva, valores inicial/final e política de movimento reduzido.
- Saída: propriedades atualizadas e indicação de que outra amostra será necessária.
- Sem dependência de GPU, wlroots, shader, blur ou material Glass. Animações devem ser testáveis com relógio controlado.
- A mesma avaliação poderá mover janelas e variar parâmetros de um efeito habilitado. O efeito não mantém relógio ou loop próprio.
- Ao desativar animações, aplicar o estado final; efeitos estáticos podem continuar habilitados.

Contrato compartilhado e regras de ativação: [docs/visual-pipeline.md](../../docs/visual-pipeline.md).
