# Janelas livres com encaixe lateral

As janelas abrem em modo livre. Abrir, fechar ou focar outro aplicativo não reorganiza nem redimensiona as demais janelas. O algoritmo de organização automática, sua proporção principal/pilha e os atalhos Super+F, Super+Espaço e Super+←/→ foram removidos. JLINUX_TILING não é mais lido e não reativa o comportamento antigo.

Arraste pelo cabeçalho e solte nos últimos 12 pixels da borda esquerda ou direita da tela: a janela ocupa a metade correspondente da área útil, descontando dock, barra e moldura. O encaixe ocorre ao soltar, não enquanto o mouse passa pela borda. Não há ação especial no topo ou embaixo, nem prévia de encaixe. O encaixe também funciona com Super+arraste.

Ao puxar a janela encaixada para longe da lateral, o tamanho anterior é restaurado. Redimensionar pela moldura mantém controle manual. Os limites mínimos e máximos informados pelo aplicativo continuam respeitados: se o mínimo não cabe na metade, a janela permanece livre; um máximo menor deixa espaço vazio. Janelas maximizadas/em tela cheia precisam ser restauradas antes do arraste. Não são alterados os tamanhos de outras janelas, mesmo que haja sobreposição.

Maximizar/restaurar pelo botão e Super+↑/↓ continuam disponíveis. Alt+Tab alterna o foco; Super+1…9 troca de espaço e Super+Shift+1…9 envia a janela. Diálogos acompanham o espaço do pai. Não há indicador de espaços no shell nesta entrega.

O cálculo é executado somente ao soltar um arraste. Não há timer ou animação nova. A implementação usa a saída central e sua área útil, como a política de maximização atual; seleção por monitor em multimonitor permanece pendente.

## Ativação e validação

Instalar o pacote não substitui o compositor já em execução. A alteração entra no próximo login. Não encerrar apenas o compositor para tentar recarregá-la, pois isso fecha a sessão e seus aplicativos.

Vinte e um testes locais release e ASan/UBSan passaram nesta base. Novo teste headless com Foot real e shell verifica ausência de reorganização ao abrir/fechar, metades com reservas de dock/barra, transição esquerda→direita, restauração ao puxar e ausência de encaixe no topo. A fixture de limites verifica que um mínimo de 900 px não é forçado em meia tela e mantém regressões de diálogos/espaços. Os testes antigos do modo automático foram removidos/substituídos. Sem benchmark, teste de VT ou nova validação física DRM/GPU.
