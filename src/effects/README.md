# effects/

glass.c implementa o material inicial do shell em Cairo: fundo do wallpaper, tonalização e reflexos estáticos independentes. Não há shaders, refração ou desfoque de janelas atrás do shell. O passe CPU de blur está em renderer/blur.c; cache e invalidação pertencem ao cliente shell.

Cada efeito terá parâmetros e ativação próprios: blur, sombra, transparência, tonalização, reflexo de borda e refração. O material Glass compõe os efeitos selecionados, sem acoplar seus controles ao motor de animações.

Efeitos descrevem os passes necessários, recursos e expansão de damage; renderer/ executa os comandos gráficos e mantém a apresentação. Detalhes desse contrato devem ser definidos ao implementar o primeiro efeito, evitando uma API fictícia antecipada.

Desativar um efeito deve retirar seu passe, invalidar a área afetada e liberar recursos exclusivos quando não estiverem mais em uso. Os outros efeitos e as animações continuam conforme suas configurações.

Contrato compartilhado e fallback: [docs/visual-pipeline.md](../../docs/visual-pipeline.md).
