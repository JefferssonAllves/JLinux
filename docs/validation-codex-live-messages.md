# Respostas Codex em tempo real

Instalado em 15/11/2026 a partir do commit bcfb049, dependente de feature/codex-project-groups (PR #124).

- 32 testes unitários passaram: transporte Unix real isolado, deltas de texto, exclusão de ferramentas/comentários/outros agentes, autenticação e fechamento do fluxo ao sair.
- Integração Playwright no compositor headless passou: texto parcial visível antes da resposta completa e saída de ferramenta privada ausente; grupos por caminho, perguntas, prompts, arquivos, mídia e tela remota preservados.
- Sintaxe JavaScript e git diff --check passaram.
- Arquivos instalados com backup em ~/.local/state/jlinux-connect/backups/live-bcfb049. Reiniciado apenas o gateway; não havia terminal/comando filho ativo. A visualização remota pode reconectar após essa atualização.
- Sessão autenticada antes da atualização continuou válida após instalar. WebSocket HTTPS privado conectado à API real e formato de mensagens filtrado verificado, sem imprimir conteúdo. Socket Codex preservou o inode: agentes não foram reiniciados. Nenhum prompt real de teste foi enviado.

O fluxo usa item/started, item/agentMessage/delta e item/completed da API do Codex 0.169.0, verificados nos schemas gerados pelo CLI instalado. Deltas sem mensagem identificada são descartados; histórico recupera texto se a conexão iniciar durante uma resposta. Atualizações agrupadas em 50 ms não garantem latência de rede. Não medimos simultaneidade ou latência em um iPhone físico.
