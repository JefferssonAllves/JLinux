# Touchpad na aba Controle

Instalado em 15/11/2026 a partir de 9ee9add; dependente de feature/iphone-remote-hub (PR #128).

A aba Controle ganhou uma área própria para movimento relativo, toque para clicar e segurar antes de arrastar. Reutiliza o WebSocket e cliente C existentes; sem transmissão de vídeo. Ponteiro conecta somente na aba Controle ou na sessão de tela compatível, fecha ao mudar de aba/minimizar e reconecta ao voltar. Indicador mostra pronto/reconectando.

Integração Playwright/headless passou com cliente C real: deslocamento relativo esperado, clique, arraste e cancelamento com botão liberado. Verificado que usar o touchpad de Controle não cria canvas de vídeo; gestos da aba Tela e fluxos anteriores também passaram. Sintaxe JavaScript e git diff --check passaram. Captura móvel inspecionada.

Atualização apenas dos arquivos públicos e README, com backup em ~/.local/state/jlinux-connect/backups/control-touchpad-9ee9add. HTML, CSS e JS instalados conferidos por HTTPS; PID do gateway e inode do socket Codex preservados. Nenhum serviço foi reiniciado, nenhuma entrada foi enviada ao desktop pessoal e o pareamento não foi alterado. Sem alterações no backend, nas suas dependências ou no compositor.

Não validado em iPhone físico; sensibilidade usa fator fixo 2 na área de Controle, sem calibração física ou benchmark de latência. Os testes desta entrega concentram-se na integração alterada; a suíte de backend não foi repetida porque o backend permaneceu igual.
