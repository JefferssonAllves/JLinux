# JLinux Connect: acesso persistente, mídia e Codex

Instalado em 15/11/2026 a partir de 443b6b1, na branch feature/iphone-codex-media, dependente de feature/iphone-system-controls (PR #121).

- 29 testes unitários passaram; sintaxe JavaScript e git diff --check passaram.
- Integração Playwright em compositor headless passou: reconexão após recarga offline sem novo pareamento, cache sem dados privados, mídia, lista/perguntas/prompts do Codex com backend simulado, imagem em tela cheia, teclado remoto, volume, arquivos e terminal.
- Instalação real preservou cookie autenticado emitido pela versão anterior: /api/status retornou 200 após reiniciar somente o gateway e a sessão confiável foi migrada para validade permanente no servidor.
- APIs reais /api/media e /api/codex/threads retornaram 200. Inode do socket Codex permaneceu igual após instalar/iniciar a unidade idempotente; nenhum agente foi reiniciado.
- Unidades jlinux-connect e jlinux-codex habilitadas. Tailscale Serve continua privado à tailnet; PC e iPhone sem expiração de chave.
- Backup anterior em /var/backups/jlinux-connect-before-codex-media-443b6b1/app.tar.gz. Pacote desktop preservado: 0.5.0.r149.g3d6bbf0-1. Não houve logout/reboot.

Não enviamos prompts reais aos agentes nem executamos ações reais de energia/atualização durante os testes. Perguntas e aprovações foram verificadas com servidor isolado; tipos não suportados orientam responder no PC. Não medimos latência, FPS ou qualidade em iPhone físico/rede celular. Tela remota exige sessão gráfica ativa; Safari pode exigir reautenticação se seus dados forem apagados.
