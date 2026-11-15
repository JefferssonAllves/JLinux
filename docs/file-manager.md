# Arquivos JLinux

Gerenciador nativo Wayland/C. A proposta gerada está em [files-complete-concept.png](design/files-complete-concept.png); a prévia renderizada está em [files-complete-native.png](design/files-complete-native.png). A implementação segue a composição e o material, sem afirmar correspondência pixel a pixel.

## Operações

Botão direito no arquivo ou no espaço vazio abre o menu: abrir no aplicativo associado, copiar, recortar, colar, renomear, nova pasta, novo arquivo, mover para lixeira, propriedades, ocultos, atualizar e terminal na pasta. Ações indisponíveis ficam desativadas. A seleção aceita Ctrl+clique, Shift+clique e Ctrl+A; copiar/recortar/colar e lixeira operam sobre a seleção. Renomear atua em um item por vez.

Ctrl+C/X/V reutilizam o menu; F2 renomeia; Delete pede confirmação para mover à lixeira. Ctrl+N cria arquivo; Ctrl+Shift+N cria pasta. Ctrl+L ou clique no caminho abre a navegação por endereço. Nas caixas, Enter confirma, Esc cancela e Ctrl+A substitui o nome. A tecla Menu e setas permitem acionar o menu pelo teclado.

Cópia e movimentação usam processos separados e argumentos individuais, preservando nomes com espaços, apóstrofos e caracteres de shell. Destinos existentes são recusados; os comandos também proíbem sobrescrita. Operações em lote podem terminar parcialmente em falhas de permissão/disco: o rodapé informa a falha e os originais restantes são preservados. Não há remoção permanente automática. Lixeira depende do suporte GIO no volume; se indisponível, a operação falha sem apagar o arquivo. O clipboard de arquivos é interno à janela; integração entre programas ainda não foi implementada.

## Miniaturas

Imagens suportadas pelo GdkPixbuf recebem prévia proporcional na lista. O cache em memória guarda até 256 resultados por navegação, incluindo resultados negativos; a atualização limpa o cache. Arquivos acima de 16 MiB ou imagens acima de 40 milhões de pixels recebem ícone. Links simbólicos não são decodificados. Outros tipos reutilizam miniaturas PNG já existentes no cache padrão do usuário, se mais novas que o arquivo. Não há geração própria de miniaturas de vídeo/PDF; tipos sem prévia recebem ícone. A decodificação de imagens ocorre na primeira exibição das linhas visíveis, na CPU; desempenho em pastas grandes não foi medido.

## Validação

Testes de ações usam diretório privado no disco do usuário e isolam a lixeira. Cobrem criação, colisões, cópia, renomeação, recorte, lixeira, nomes literais e cópia múltipla. Teste de miniaturas cobre dimensões, cache, arquivo inválido e symlink. Teste headless verifica navegação, diálogos e abertura de arquivos; não comprova pareamento visual exato nem desempenho de hardware.
