# Codex: finalizado não lido

Instalado em 15/11/2026 a partir de 3775139; branch dependente de feature/codex-live-messages (PR #125).

- 33 testes unitários passaram: somente execução completed sem erro gera marcador; execução ativa/interrompida/com falha não gera; novas execuções têm marcadores distintos e metadados estáveis usam cache limitado.
- Playwright em compositor headless passou: filtro e identificação de finalizado não lido, aviso persistente após recarregar, abertura da conversa limpa aviso e leitura continua salva após outra recarga. Fluxos anteriores de grupos, prompts, transmissão parcial, mídia e tela remota passaram.
- Sintaxe JavaScript e git diff --check passaram. Consulta real retornou 8 agentes e 7 conclusões bem-sucedidas em 0,332 s; sem enviar prompts, imprimir mensagens ou alterar agentes.
- Arquivos instalados com backup em ~/.local/state/jlinux-connect/backups/completion-3775139. Reiniciado somente gateway, sem terminal/comando filho ativo. Sessão autenticada anterior continuou válida; API instalada retornou marcadores e arquivo público coincidiu com a fonte. Inode do socket Codex preservado.

Leitura registrada no localStorage deste navegador por ID da conversa e da execução, sem conteúdo de mensagens. Ler no PC não marca no celular. Apagar dados do navegador remove as marcas; histórico concluído sem marca local aparece inicialmente como não lido. Não houve validação em iPhone físico. Conclusão do Codex indica que a execução terminou com sucesso, não comprova por si só que todo objetivo do usuário foi alcançado.
