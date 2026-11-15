# Controle remoto compacto no iPhone

Instalado em 15/11/2026 a partir de 5ed01b9, dependente de feature/mobile-touchpad-fullscreen (PR #127).

- Navegação reduzida de seis para quatro abas: Controle, Tela, Codex e Mais. Arquivos, terminal e sistema preservados em Mais; desconectar fica em Sistema.
- Controle reúne volume/mudo, player MPRIS, direcionais/Enter, Esc/Tab/Alt+Tab e nove apps do dock com ícones reais instalados. Ícones de Spotube/WhatsApp seguem personalizações nativas. Faixa horizontal mantém a tela compacta; captura de vídeo continua sob demanda.
- 38 testes unitários passaram: índices do dock enviados por socket privado, validação de IDs/teclas, ícones por metadados e fallback, proteção das novas rotas e fluxos anteriores.
- Playwright/headless passou: quatro abas, ícones carregados, ações do dock com destino simulado, direção esquerda via wtype real em compositor isolado, OK, volume, mídia, agrupamento/perguntas/prompts/streaming Codex, touchpad/tela cheia, arquivos, terminal e confirmações do sistema. Captura móvel inspecionada; sem transbordamento horizontal.
- Sintaxe JS/shell e git diff --check passaram. Instalador oficial aplicado; backup em /var/backups/jlinux-connect-before-remote-hub-5ed01b9/app.tar.gz. Sessão autenticada anterior preservada; nove ícones protegidos e assets conferidos por HTTPS. Somente gateway reiniciado; socket Codex preservado. Não foram enviados atalhos ou lançados apps no desktop pessoal durante a validação.

O catálogo segue os índices da API do dock da instalação 0.5.0.r149.g3d6bbf0-1; deve acompanhar alterações futuras desse protocolo. Configurações é painel do shell, não índice de app lançável. Resposta ao atalho confirma envio ao compositor, não abertura concluída. Direcionais dependem do suporte a teclado do app focado. Não houve teste em iPhone físico nem benchmark de latência.
