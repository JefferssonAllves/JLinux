# Gravador de tela: validação e instalação

Implementação ea2e730, baseada na instalação anterior 3d6bbf0 (PR #120, feature/files-context-thumbnails). Instalada em 17/11/2026 pelo pacote jlinux-desktop-git 0.5.0.r150.gea2e730-1; wf-recorder 0.6.0-2 instalado como dependência. Meson/GCC com warnings tratados como erros, wlroots 0.20.2.

## Evidências

- 51/51 testes release passaram, incluindo Print/clipboard e o novo screen-recording. O makepkg oficial repetiu os 51 checks com sucesso usando o commit local exato já enviado ao GitHub.
- Integração em compositor headless/pixman: Shift+Print iniciou, outro Shift+Print finalizou, ffprobe confirmou MP4 H.264 1280×720 com duração/frames positivos e arquivo privado. Nova gravação recebeu outro nome; encerrar o shell finalizou o segundo vídeo. Encoder simulado com falha não deixou arquivo vazio.
- Capturas de teste inspecionadas: indicador vermelho REC/tempo à direita durante a gravação, ausente após parar. Nenhum vídeo da sessão pessoal foi capturado. Consulta somente de saídas confirmou um monitor HDMI-A-1 na sessão atual.
- Build separado ASan/UBSan passou o teste screen-recording; ASAN_OPTIONS=detect_leaks=0, sem alegar verificação de vazamentos de bibliotecas externas.
- git diff --check e pacman -Qkk passaram.

## Instalação e ativação

Pacote construído em ~/.cache/jlinux-packages/screen-recorder-ea2e730 e preservado em /var/cache/pacman/pkg. Backup dos arquivos anteriores e metadados pacman em /var/backups/jlinux-before-screen-recorder-ea2e730/.

As identidades dos três processos existentes (supervisor, compositor, shell) foram comparadas antes/depois e preservadas. **O pacote novo está instalado; o atalho e o indicador serão ativados no próximo login.** Não houve logout, reinício, troca de VT ou encerramento de aplicativos. Essa distinção é necessária porque atalhos e renderização da barra pertencem aos binários C já carregados; não há hot reload seguro implementado para eles.

Vídeo sem áudio, saída padrão do wf-recorder; seleção multimonitor e captura de som não implementadas. Não houve gravação do monitor físico nem medição de CPU/GPU/FPS/latência no hardware pessoal. Finalização forçada pode deixar arquivo parcial, preservado quando não vazio. Veja docs/screen-recording.md.
