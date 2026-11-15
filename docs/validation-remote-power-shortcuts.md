# Energia no topo do Controle

Instalado em 15/11/2026 a partir de de311f6, dependente de feature/control-touchpad (PR #129).

Desligar, Reiniciar e Sair da sessão no topo da aba Controle usam os mesmos handlers, preparação, confirmação e bloqueios das ações existentes em Mais → Sistema. Estado de atualização consultado ao entrar no Controle, sem novo polling periódico.

Integração Playwright/headless passou: cada novo atalho abriu confirmação, cancelar os três não executou ações e a atualização simulada bloqueou reinício/desligamento nas duas áreas. Touchpad, tela, mídia, agentes, arquivos e terminal preservados. Sintaxe JS e git diff --check passaram.

Arquivos públicos instalados e conferidos por HTTPS; backup em ~/.local/state/jlinux-connect/backups/power-shortcuts-de311f6. PID do gateway e inode do socket Codex preservados. Não houve reinício de serviço, logout, reboot ou desligamento real. Backend sem alterações; não foi repetida a suíte de backend. Não validado em iPhone físico.
