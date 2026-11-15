# JLinux Connect — validação local

Instalado o compositor `jlinux-desktop-git 0.5.0.r148.g5b28579-1`, preservando o suporte Print/clipboard de f74654e e a lista de dez aplicativos do dock. O companion foi instalado separadamente pelo instalador em `extras/connect`, com CLI de pareamento/QR local, nove testes de autenticação e backend, e serviço de usuário habilitado.

## Resultado

- 48 testes nativos passaram, incluindo Print/clipboard e opt-in dos protocolos de entrada remota.
- Nove testes do companion passaram: chave privada, proteção de rotas HTTP/WebSocket, Host/Origin, limites de tamanho, tentativas de login, expiração, validação de áudio, ponte binária, logout, revogação durante inicialização, terminal PTY real e QR local. Alguns testes cobrem múltiplos casos.
- Integração Playwright em viewport 390×844 passou com captura e entrada reais em compositor headless separado: login, volume simulado, captura, digitação numa janela, terminal e logout.
- Testado HTTPS através do Tailscale Serve, com certificado validado normalmente, login, cookie Secure/HttpOnly, consulta real do volume e logout. Nenhuma porta TCP de backend foi aberta; gateway apenas em 127.0.0.1:8765.
- Verificação do pacote instalado: 34 arquivos, nenhum alterado.
- Serviço sem captura/terminal ativos usou cerca de 27 MB de MemoryCurrent nesta medição, excluindo o daemon Tailscale e os backends sob demanda. Não representa o consumo com transmissão de tela ou terminal em uso.

## Ativação e limites

Conta Tailscale vinculada, Serve/HTTPS habilitado e proxy privado configurado. iPhone cadastrado, mas desconectado na última consulta. O QR foi aberto em uma janela própria; a chave não foi copiada para o repositório ou relatório.

A sessão gráfica que estava aberta na instalação ainda não anunciava captura/entrada remota. Precisa de novo login; não reiniciamos o compositor nem encerramos os aplicativos do usuário. Volume e terminal já estavam acessíveis pelo gateway.

Não testado em Safari/iPhone físico nem por 4G/5G. O computador precisa estar ligado e logado para a tela. Volume controla a saída local; áudio não é transmitido. Terminal opera com as permissões da conta do usuário.

Backup anterior do desktop: `/var/backups/jlinux-before-iphone-connect-5b28579/files.tar.gz`.
