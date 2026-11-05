# renderer/

Código atual: criação e limpeza do renderizador/alocador wlroots e apresentação da cena com frame-done monotônico. compositor/main.c mantém eventos de saída e delega o trabalho gráfico ao módulo.

Responsabilidades a evoluir: execução ordenada de passes, buffers/texturas, sincronização, damage, caches e elegibilidade de direct scanout. Ainda não há renderizador de efeitos próprio ou shaders Glass.

Não decide quais preferências do usuário reduzir. Recebe o estado visual resolvido e renderiza; não calcula curvas de animação ou altera configurações de qualidade por conta própria.

Fronteiras: [docs/visual-pipeline.md](../../docs/visual-pipeline.md).

blur.c: passe separável de box blur ARGB32 usado pelo cache reduzido do wallpaper no shell. Teste de impulso, cor constante, bordas e entradas inválidas em tests/blur.c. Não é blur GPU do compositor.
