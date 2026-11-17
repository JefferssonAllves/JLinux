# Gravador de tela

- **Shift+Print Screen** inicia; pressione novamente para encerrar e finalizar o arquivo.
- Um ponto vermelho e **REC mm:ss** aparecem à direita da barra superior enquanto o processo está ativo. Em telas estreitas, fica somente o ponto para preservar o relógio e os demais controles.
- Vídeo **MP4/H.264, 30 FPS, sem áudio**, salvo na pasta de vídeos configurada no XDG, subpasta **Gravações**; sem configuração XDG, usa ~/Videos/Gravações. Nomes recebem data, hora e sufixo exclusivo, sem substituir gravações anteriores.
- Print sem Shift continua copiando a captura PNG para a área de transferência.

O shell nativo C gerencia o wf-recorder instalado pelo pacote, sem comando livre de shell. Mantém uma gravação por sessão, sinaliza SIGINT ao parar e aguarda o filho finalizar antes de remover o indicador. Erros removem apenas arquivos vazios; arquivos parciais não vazios são preservados. Encerrar o shell solicita finalização; se o gravador não responder em dois segundos, ele é encerrado à força, podendo deixar um MP4 parcial. Queda de energia/encerramento abrupto também pode impedir a finalização do arquivo.

Não inicia gravação ao fazer login. A barra usa o temporizador já existente, com atualização de um segundo somente durante a gravação; o contador usa tempo monotônico. Não há microfone, transmissão pela rede, editor de vídeo ou serviço persistente novo. A gravação usa a saída padrão do wf-recorder; seleção de múltiplos monitores e áudio não fazem parte desta versão. O codificador usa libx264/ultrafast e qualidade CRF 24; não há alegação de vantagem medida de CPU/GPU ou latência.

O atalho e o indicador são nativos: após atualizar o pacote, **saia e entre novamente na sessão para ativar**. A instalação não encerra os aplicativos abertos nem reinicia o computador.
