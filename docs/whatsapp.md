# WhatsApp no JLinux

O app abre o WhatsApp Web oficial em uma única janela Wayland/WPE, com moldura do compositor. O executável jlinux-whatsapp é um link para o navegador existente em modo dedicado: usa as bibliotecas WPE já instaladas, sem outro motor distribuído junto. Não há abas, barra de endereços, processo de inicialização Python ou bloqueador de anúncios carregado neste modo. Não é o aplicativo desktop oficial da Meta.

Abra `jlinux-whatsapp` ou o atalho WhatsApp do catálogo de aplicativos. No celular, use Aparelhos conectados para vincular pelo QR code. A janela de vinculação foi verificada com o serviço real; envio/recebimento de mensagens e persistência de uma conta real dependem da vinculação pelo usuário e continuam sem validação nesta entrega. Chamadas, câmera, gravação por microfone, notificações e envio de anexos não são capacidades validadas desta primeira versão. Downloads reaproveitam o suporte do navegador. Ctrl+R/F5 recarrega e Ctrl+W/Ctrl+Q fecha; Ctrl+T não cria aba.

O perfil fica em `$XDG_DATA_HOME/jlinux/whatsapp` (padrão ~/.local/share/jlinux/whatsapp), separado do navegador, e o cache em `$XDG_CACHE_HOME/jlinux/whatsapp/webkit`. Diretórios privados 0700, cookies persistentes e armazenamento web reaproveitam a implementação validada do navegador. Um lock impede instâncias concorrentes do mesmo perfil, para evitar duplicação de renderizadores; um segundo lançamento informa que o app já está aberto, sem trazer a janela à frente. Fechar a janela encerra o app; não há cliente em segundo plano.

O sandbox e a recusa de erros TLS são preservados. Apenas HTTPS web.whatsapp.com (ou seus blobs) pode ocupar o conteúdo principal sem barra de endereço; links HTTP/HTTPS externos acionados pelo usuário vão para o navegador padrão. Avisos de erro aparecem no título da moldura nativa e no log, mesmo sem barra de navegação. Câmera, microfone e localização continuam recusados, conforme o suporte atual do navegador. O ícone W é próprio e não uma reprodução do logotipo oficial.

## Memória: resultado e limites

**O objetivo de consumo baixíssimo não foi alcançado.** O cliente elimina interface e instâncias extras, mas a aplicação web do WhatsApp ainda pesa centenas de MiB. Não há evidência de que esta seja a opção de menor consumo entre todos os clientes existentes; ela reutiliza a infraestrutura já presente nesta instalação.

Medições de 06/11/2026, WPE 2.54.2, build release, perfil novo e conta não vinculada. Duas execuções de 25 segundos por condição, amostragem de 0,5s, PSS de todos os processos descendentes (incluindo os iniciados por threads) e SwapPss quando registrado. A mediana usa os últimos oito segundos. Backend Wayland headless 1280×720, compositor GLES2 e driver GL padrão do cliente; não é medição DRM da sessão física nem contabilização de toda memória da GPU. O compositor fica fora da árvore medida. Outros aplicativos permaneceram na máquina; página remota, cache do sistema e processos compartilhados podem causar variação. A janela real do WhatsApp será aberta separadamente para vinculação manual.

| Condição | PSS mediano pré-login | Maior pico amostrado | CPU nos últimos 8s (100% = um núcleo) |
| --- | --- | --- | --- |
| Versão entregue, GLES2 | 620–632 MiB | 705 MiB | 2,89–3,03% |
| Piloto de pressão antecipada, não adotado | 558–621 MiB | 678 MiB | 5,65–5,80% |

Um piloto inicial de pressão antecipada chegou a 373–380 MiB, mas essa redução não se repetiu de forma estável e a CPU aumentou. Foi rejeitado. O piloto usava WebKitWebContext com memory-pressure-settings: referência de 512 MB por processo, limiares conservador 0,25/estrito 0,5, intervalo 5s e kill-threshold=0. Esses parâmetros não estão no app entregue. O ensaio inicial totalmente por software ficou próximo de 800 MiB; não deve ser confundido com o ensaio GLES2. A versão entregue usa somente o modelo de cache DOCUMENT_VIEWER da API pública, apropriado a uma janela sem histórico de navegação.

[Dados brutos sanitizados](performance/whatsapp-memory-2026-11-06.json). Não incluem cookies, mensagens, QR codes ou identificadores de conta. Os arquivos com CPU registram o SHA-256 dos binários medidos; o piloto foi compilado separadamente e não integra a instalação.

Reproduzir a condição entregue, com acesso de leitura aos contadores dos próprios processos (sudo -n é usado apenas se necessário para ler smaps_rollup de descendentes sandboxed):

```sh
scripts/measure-whatsapp.py build-native/jlinux-compositor build-native/jlinux-browser /tmp/whatsapp-memory.json gles2
```

Não executa login nem altera áudio/sessão de login. Usa perfis de teste descartáveis em compositor headless e preserva o sandbox. O estado depois de vincular, conversar, ver vídeos e manter horas em repouso não foi medido; esses resultados não representam teto de RAM nem comparação com Electron/Chromium. Referências: [WhatsApp Web](https://web.whatsapp.com/), [browsers suportados](https://faq.whatsapp.com/852892549070029/) e [política de pressão de memória do WebKit](https://webkitgtk.org/reference/webkit2gtk/unstable/struct.MemoryPressureSettings.html).
