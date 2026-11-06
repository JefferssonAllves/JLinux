# Arquivos JLinux — versão inicial

Cliente separado em C, Wayland direto e toolkit Cairo/Pango do JLinux. Reutiliza o material de effects/ na barra interna e as molduras existentes do compositor. Não usa GTK, CSS ou Python no runtime. O corpo é opaco; não implementa blur sobre outras janelas nem o Glass completo.

Execute `jlinux-files [pasta]`, ou use o primeiro ícone do dock na nova versão do shell. Sem argumento abre a pasta pessoal. A atualização do pacote não substitui o shell já em execução: o novo ícone aparece no próximo login. O aplicativo pode ser aberto na sessão existente sem logout.

- Clique seleciona; duplo clique ou Enter abre.
- Setas selecionam, roda do mouse rola, Backspace/Alt+Seta acima sobe uma pasta.
- Botões: pasta pessoal, subir e atualizar. F5 atualiza; Ctrl+H alterna ocultos.
- Pastas aparecem primeiro. Links para pastas são navegáveis; falhas preservam a pasta atual.
- Arquivos abrem pelo aplicativo associado através de `gio open`, com argumentos separados, sem shell. Associações/aplicativos devem existir no Arch; falhas aparecem na barra de estado.

Esta versão não copia, move, renomeia ou exclui arquivos. Não monitora alterações externas: use F5. Listagem síncrona limitada a 50 mil itens; pastas lentas/remotas podem bloquear temporariamente este aplicativo. Não há benchmark de desempenho ou validação de acessibilidade completa. Renderização por buffers SHM ocorre por eventos; não é um renderer acelerado novo.

A prévia em design/native-files-preview.png usa arquivos fictícios e o mesmo desenho do aplicativo; não é captura da pasta pessoal ou da sessão real. Testes headless cobrem navegação, teclado, clique, abertura por associação com nomes especiais e lançamento pelo dock. Teste de modelo cobre links, ocultos, nomes inválidos em UTF-8 e preservação após erro.

O fundo atual é um gradiente opaco azul-ardósia, sem preto predominante; barra interna usa contorno contínuo azul/violeta sem pontos focais. Transparência e blur do conteúdo continuam pendentes.

## Barra lateral de locais

Atalhos fixos à esquerda: Pessoal, Downloads, Documentos, Imagens e Vídeos. Um clique navega; Alt+1…5 segue a mesma ordem. A pasta atual recebe destaque. Caminhos seguem user-dirs.dirs via GLib, incluindo pastas personalizadas fora da home; sem configuração, usam nomes padrão Downloads/Documents/Pictures/Videos dentro da home. Pastas ausentes ficam esmaecidas; a tentativa mostra erro e preserva a pasta atual. Nenhuma pasta é criada automaticamente.

Lista e área de clique ficam à direita da barra lateral; a roda sobre a barra não rola a lista. Tamanho mínimo passa a 540×360 para manter os cinco atalhos legíveis; abaixo de 760 px de largura a coluna de tamanho fica oculta. A prévia usa apenas arquivos fictícios.

## Pastas dos usuários existentes — configuração local

Em 05/11/2026 foram criadas Downloads, Documentos, Imagens e Vídeos nas homes dos dois usuários locais regulares existentes (UIDs 1000 e 1001). Pessoal aponta para a própria home, sem subpasta duplicada. Os diretórios foram criados como cada usuário, com modo 0700; nenhum arquivo foi movido, excluído ou renomeado. As duas contas não tinham configuração user-dirs.dirs anterior.

Em cada home, ~/.config/user-dirs.dirs foi criado com modo 0600 e propriedade do usuário:

```sh
XDG_DOWNLOAD_DIR="$HOME/Downloads"
XDG_DOCUMENTS_DIR="$HOME/Documentos"
XDG_PICTURES_DIR="$HOME/Imagens"
XDG_VIDEOS_DIR="$HOME/Vídeos"
```

A resolução foi conferida via g_get_user_special_dir da GLib, sob cada conta, juntamente com acesso de leitura/escrita/travessia e propriedade dos diretórios/configuração. Arquivos foi reaberto na sessão atual para renovar o cache GLib. Esta é uma configuração local aplicada aos usuários existentes, não um provisionador automático de novas contas. Os arquivos privados de configuração não são versionados; este registro descreve a alteração aplicada. Não houve atualização de pacote ou reinício do desktop por esta entrega.
