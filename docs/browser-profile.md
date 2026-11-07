# Perfil persistente do navegador

A navegação normal usa `webkit_network_session_new()` com perfil em `$XDG_DATA_HOME/jlinux/browser` (padrão `~/.local/share/jlinux/browser`) e cache separado em `$XDG_CACHE_HOME/jlinux/browser/webkit` (padrão `~/.cache/jlinux/browser/webkit`). Cookies persistentes ficam em `cookies.sqlite`; localStorage e IndexedDB são gerenciados pelo próprio WPE. As abas compartilham a sessão/perfil.

As duas pastas são criadas/restringidas a 0700, com verificação de proprietário e rejeição de symlink no diretório final. Se não for possível preparar o perfil, o navegador informa erro e não inicia com uma sessão temporária silenciosa. Nenhum cookie é convertido em cabeçalho manual, nem tem validade estendida artificialmente. TLS/sandbox e bloqueio de anúncios continuam usando as políticas existentes.

É preciso fazer login uma vez na versão atualizada. A versão anterior guardava tudo em memória e esses dados não são migrados do processo já aberto. Depois disso, sessões persistentes podem ser reutilizadas entre aberturas. Sites podem encerrar/revogar a sessão, exigir autenticação adicional ou usar cookies que duram apenas uma sessão; esse prazo continua sob controle do site. Esta entrega não é um cofre de senhas nem restaura abas automaticamente.

O modo `--probe` permanece efêmero: não lê os cookies do usuário nem reutiliza localStorage/IndexedDB do perfil normal. Limpar somente o cache não apaga o perfil de login. O perfil contém dados de autenticação e deve ser mantido privado, como os demais dados da conta do sistema.

## Validação local

`web-browser-profile` usa servidor HTTP local e pastas XDG isoladas. Salva cookie HttpOnly com Max-Age, localStorage e IndexedDB; fecha o navegador pelo SIGTERM tratado no loop normal; remove o cache; reabre e confere os três dados. Um segundo perfil não recebe esses dados; `--probe` não recebe os cookies persistentes. O teste confere modo 0700 e recusa perfil redirecionado por symlink sem alterar o alvo.

O teste passou em release e ASan/UBSan (`detect_leaks=0`). Navegação/downloads/Blob, vídeo, bloqueador, endereços e motor/TLS passaram nos testes existentes. Não foi validada uma conta real do usuário nem feito novo benchmark de memória. O padrão de criação de sessão/perfil e armazenamento SQLite segue o [MiniBrowser WPE do WebKit](https://github.com/WebKit/WebKit/blob/webkitgtk-2.54.0/Tools/MiniBrowser/wpe/main.cpp).

Instalado em 06/11/2026 pelo pacote `jlinux-browser-git 0.1.0.r60.g416a887-1` (commit 416a887). Backup do executável anterior em `/var/backups/jlinux-browser-before-persistent-416a887/`. Pacman confirmou os arquivos do pacote sem alterações. A janela aberta foi preservada; reabrir o navegador e fazer login uma vez para começar a usar o perfil persistente. Não requer sair da sessão do desktop.
