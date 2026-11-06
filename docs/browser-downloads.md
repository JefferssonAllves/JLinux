# Downloads no navegador

Links de download e respostas HTTP/HTTPS de arquivos não exibidos pelo motor são salvos na pasta XDG Downloads (fallback ~/Downloads). O navegador cria a pasta se necessário, não executa arquivos e não abre o resultado automaticamente.

A seta ↓ à direita do endereço abre Downloads com o aplicativo padrão de pastas; Ctrl+J faz o mesmo. Durante transferências, mostra a quantidade ativa e a média de progresso estimado pelo WPE. ✓ indica conclusão e ! indica falha na última transferência encerrada. Detalhes e caminhos aparecem no registro do processo. Não há histórico persistente, pausa/retomada nem escolha de destino nesta versão. Fechar a última aba cancela transferências em andamento.

Nomes sugeridos são reduzidos ao nome-base; colisões com arquivos existentes, links simbólicos ou downloads ativos ganham prefixos 1-, 2-, etc. A sobrescrita permanece desabilitada no motor para proteger também contra corridas entre processos. Falhas de escrita aparecem como falha, sem executar o arquivo. Downloads de subframes não suportados não são iniciados automaticamente.

Validação: teste Wayland isolado salva duas cópias, preserva arquivo preexistente, verifica os bytes, provoca falha de destino e abre a pasta via handler padrão de teste. Exercita também abas, navegação e resize. Não mede memória nem comprova compatibilidade com todos os sites, redirecionamentos ou downloads autenticados.

Instalado em 06/11/2026 pelo pacote `jlinux-browser-git 0.1.0.r55.g5a9a774-1` (commit 5a9a774). Backup do executável anterior em `/var/backups/jlinux-browser-before-downloads/`. Reabrir o navegador para carregar a atualização. O desktop recebeu a associação padrão de pastas pelo pacote `0.5.0.r69.g850294f-1`. Compilação e empacotamento locais concluídos; testes de downloads/interação, vídeo e bloqueador passaram em headless, sem validação visual na sessão real nem benchmark de memória.

## Imagens geradas pela página (Blob)

A política de navegação agora aceita URLs `blob:http://` e `blob:https://` originadas no conteúdo web, com validação da URL interna e limite de tamanho. O acesso ao objeto segue as regras de origem do próprio WebKit. Isso permite links `<a download>` de imagens geradas ou recebidas via JavaScript. A barra de endereço continua recusando URLs blob, file, data e javascript; a alteração não habilita leitura de arquivos locais.

Regressão reproduzida em página local: o navegador anterior (5a9a774) não salvava a imagem Blob; a correção salva e verifica os bytes. O teste cobre PNG criado via Blob, além dos downloads HTTP, colisões de nomes e falha de destino já existentes. Não houve acesso à conta do usuário para comprovar o fluxo real do ChatGPT. Janelas adicionais (`target=_blank`/`window.open`) continuam bloqueadas nesta versão; um site que dependa desse fluxo precisa de investigação separada.
