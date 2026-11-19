# Inicialização sem monitor

O login continua exigindo senha. O Cage do greeter inicia um backend headless antes do DRM; o modo `last` escolhe o monitor físico quando disponível e mantém a tela virtual na ausência dele.

O compositor JLinux cria uma saída virtual de 1920×1080 somente se nenhum monitor estiver ativo na inicialização. Ao conectar um monitor físico, a saída virtual é removida. A aceleração gráfica e os dispositivos de entrada da sessão DRM continuam disponíveis.

O teste `headless-startup` inicia com zero saídas, verifica o painel e captura um PNG de 1920×1080. Um segundo cenário confirma que uma saída existente é preservada sem duplicação. A validação de boot físico depende de reiniciar a máquina sem monitor.
