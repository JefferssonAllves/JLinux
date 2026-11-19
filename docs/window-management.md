# Janelas livres com encaixe lateral e no topo

As janelas abrem em modo livre. Abrir, fechar ou focar outro aplicativo não reorganiza nem redimensiona as demais janelas. O algoritmo de organização automática, sua proporção principal/pilha e os atalhos Super+F, Super+Espaço e Super+←/→ foram removidos. JLINUX_TILING não é mais lido e não reativa o comportamento antigo.

Arraste pelo cabeçalho e solte nos últimos 12 pixels da borda esquerda ou direita da tela: a janela ocupa a metade correspondente da área útil, descontando dock, barra e moldura. O encaixe ocorre ao soltar, não enquanto o mouse passa pela borda. Ao levar o ponteiro à barra superior e soltar, a janela maximiza na área útil, mantendo a barra visível. No topo, a prévia ocupa toda essa área; nas laterais, a metade correspondente. Não há ação especial embaixo. O encaixe também funciona com Super+arraste.

Ao puxar a janela encaixada para longe da lateral, o tamanho anterior é restaurado. Redimensionar pela moldura mantém controle manual. Os limites mínimos e máximos informados pelo aplicativo continuam respeitados: se o mínimo não cabe na metade, a janela permanece livre; um máximo menor deixa espaço vazio. Janelas maximizadas/em tela cheia precisam ser restauradas antes do arraste. Não são alterados os tamanhos de outras janelas, mesmo que haja sobreposição.

Maximizar/restaurar pelo botão e Super+↑/↓ continuam disponíveis. Alt+Tab alterna o foco; Super+1…9 troca de espaço e Super+Shift+1…9 envia a janela. Diálogos acompanham o espaço do pai. Não há indicador de espaços no shell nesta entrega.

A prévia é atualizada durante o arraste; ao soltar, o encaixe desliza e redimensiona suavemente por 220 ms, sem timer adicional. A implementação usa a saída central e sua área útil, como a política de maximização atual; seleção por monitor em multimonitor permanece pendente.

## Ativação e validação

Instalar o pacote não substitui o compositor já em execução. A alteração entra no próximo login. Não encerrar apenas o compositor para tentar recarregá-la, pois isso fecha a sessão e seus aplicativos.

Vinte e um testes locais release e ASan/UBSan passaram nesta base. Novo teste headless com Foot real e shell verifica ausência de reorganização ao abrir/fechar, metades com reservas de dock/barra, transição esquerda→direita, restauração ao puxar e maximização ao soltar no topo, restauração e limite vertical do cabeçalho. A fixture de limites verifica que um mínimo de 900 px não é forçado em meia tela e mantém regressões de diálogos/espaços. Os testes antigos do modo automático foram removidos/substituídos. Sem benchmark, teste de VT ou nova validação física DRM/GPU.

O dock foi removido: encaixe e maximização usam toda a largura disponível, sem margem lateral reservada.

## Encaixe com deslizamento e redimensionamento

A janela percorre uma transição smoothstep de 220 ms da posição/tamanho de soltura até a metade escolhida. O encaixe no topo reutiliza a maximização existente, sem acrescentar uma transição de tamanho. É uma transformação da cópia dos buffers já desenhados, não uma sequência de redimensionamentos enviada ao cliente. O aplicativo recebe apenas o tamanho final; durante o trajeto o conteúdo aparece como imagem temporária e ao terminar volta ao conteúdo vivo. Isso pode esticar brevemente o texto durante a transição; não há simulação física ou blur novo.

Os snapshots pertencem a cada janela e não recebem entrada. Novo clique, posicionamento manual, maximização, fullscreen, mudança de parent/workspace, unmap/destruição e saída do compositor liberam o recurso temporário e restauram a janela real. Movimento reduzido e falha ao criar snapshot aplicam o encaixe imediato. Só há agendamento de frames enquanto uma transição estiver ativa.

Testes de renderer verificam posição e dimensões no início, meio e fim, com retenção/liberação dos buffers. Headless cobre a conclusão dos dois lados, restauração, ausência de rearranjo automático, modo reduzido e desconexão logo após encaixar. A fluidez final em DRM/GPU ainda depende de avaliação no hardware; sem benchmark nesta entrega. Ativação da versão instalada no próximo login, preservando a sessão atual.


### Sincronização e recorte do conteúdo

A subárvore da superfície principal e seus subsurfaces agora é recortada à geometria xdg declarada, incluindo origens negativas como o cabeçalho do navegador. Popups xdg independentes não recebem esse recorte. Isso mantém buffers maiores que a janela dentro da moldura, tanto na cena viva quanto na cópia animada.

O fim dos 220 ms não basta para revelar o conteúdo vivo: quando há mudança de tamanho, a transição aguarda o commit que confirma o configure solicitado. Até isso ocorrer, permanece a imagem no destino; a espera não mantém frames agendados em loop e o commit desperta a atualização. Trocar entre metades com dimensões iguais não espera um redimensionamento desnecessário. Clientes que não respondem mantêm a imagem anterior até responder ou encerrar; não há afirmação de fluidez para aplicativo travado.

Clicar em outra janela não encerra todas as transições. Ao desencaixar, o ponto vertical de arraste é preservado e o estado de tamanho do próximo gesto usa o tamanho solicitado, evitando reaproveitar o tamanho antigo enquanto o cliente ainda atualiza. A posição continua acompanhando os eventos do cursor diretamente, sem acrescentar um filtro de atraso.

## Limite superior durante o arraste

Arrastar pelo cabeçalho ou com Super+botão esquerdo limita a posição vertical para que o cabeçalho fique abaixo da barra. A elevação visual de 3 px também é limitada nessa borda. A posição horizontal continua livre. Chegar com o ponteiro à barra mostra a prévia completa; maximizar ocorre somente ao soltar, preservando o tamanho anterior para restaurar. O topo tem prioridade sobre as laterais nos cantos. Fullscreen solicitado pelo aplicativo continua independente e pode ocupar a tela inteira.

Teste headless com Foot real cobre prévia antes de soltar, dimensões finais 1274×645 em 3,72 numa saída 1280×720 com barra de 36 px, restauração do tamanho e fechamento pelo cabeçalho limitado, tanto com animações quanto com movimento reduzido. Sem teste físico de DRM/VT ou benchmark.
