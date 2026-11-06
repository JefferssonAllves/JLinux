# Downloads no navegador

Links de download e respostas HTTP/HTTPS de arquivos não exibidos pelo motor são salvos na pasta XDG Downloads (fallback ~/Downloads). O navegador cria a pasta se necessário, não executa arquivos e não abre o resultado automaticamente.

A seta ↓ à direita do endereço abre Downloads com o aplicativo padrão de pastas; Ctrl+J faz o mesmo. Durante transferências, mostra a quantidade ativa e a média de progresso estimado pelo WPE. ✓ indica conclusão e ! indica falha na última transferência encerrada. Detalhes e caminhos aparecem no registro do processo. Não há histórico persistente, pausa/retomada nem escolha de destino nesta versão. Fechar a última aba cancela transferências em andamento.

Nomes sugeridos são reduzidos ao nome-base; colisões com arquivos existentes, links simbólicos ou downloads ativos ganham prefixos 1-, 2-, etc. A sobrescrita permanece desabilitada no motor para proteger também contra corridas entre processos. Falhas de escrita aparecem como falha, sem executar o arquivo. Downloads de subframes não suportados não são iniciados automaticamente.

Validação: teste Wayland isolado salva duas cópias, preserva arquivo preexistente, verifica os bytes, provoca falha de destino e abre a pasta via handler padrão de teste. Exercita também abas, navegação e resize. Não mede memória nem comprova compatibilidade com todos os sites, redirecionamentos ou downloads autenticados.
