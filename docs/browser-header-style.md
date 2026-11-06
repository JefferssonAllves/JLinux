# Cabeçalho do navegador

O botão de nova aba acompanha a última aba visível. Quando há mais abas do que espaço, acompanha a última do grupo visível; as setas e os atalhos continuam disponíveis. Pintura e clique compartilham a mesma posição calculada.

A base do cabeçalho usa RGBA (.006, .009, .016, .88), a mesma tonalização preta da decoração em feature/black-window-headers, sem moldura Glass própria nem margem transparente. A composição sobre fundos diferentes pode alterar a cor percebida.

Validação: compilação release, teste web-browser-wayland (abas, cliques, maximização e redimensionamento) e prévia Cairo. Não houve comparação de uma janela completa na sessão do usuário com as fotos de referência. Nenhum benchmark novo.
