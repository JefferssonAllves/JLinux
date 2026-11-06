# Organização automática de janelas

O compositor organiza xdg-toplevel por padrão, em C: uma janela ocupa a área útil; duas ficam lado a lado; as demais formam uma pilha vertical à direita. A principal recebe 60% da largura. As molduras entram no cálculo, sem cobrir as reservas do painel/dock na saída central.

A ordem de organização usa uma lista separada da ordem de foco. Clicar ou usar Alt+Tab não reorganiza as janelas. Abrir/fechar, alterar limites de tamanho, mudar a área útil e usar os atalhos abaixo recalculam o layout. Não há timer, animação nova, busca combinatória ou alocação no cálculo: três percursos lineares, com espaço auxiliar constante. Configurações de tamanho e estado tiled iguais às já agendadas não são reenviadas.

| Ação | Atalho |
| --- | --- |
| Promover janela focada a principal | Super+Espaço |
| Diminuir/aumentar largura da principal | Super+← / Super+→ |
| Alternar flutuante/automática | Super+F |
| Trocar espaço de trabalho (nove disponíveis) | Super+1…9 |
| Enviar janela para outro espaço | Super+Shift+1…9 |
| Maximizar / restaurar e retornar ao layout | Super+↑ / Super+↓ |
| Alternar foco no espaço atual | Alt+Tab |

A divisão varia em passos de 5%, entre 20% e 80%, compartilhada entre os espaços. Arrastar pelo cabeçalho ou redimensionar manualmente torna a janela flutuante; Super+F permite reinseri-la. Diálogos com parent xdg ficam flutuantes, centrados no pai ao mapear e seguem seu espaço quando o pai é enviado. Não há regras persistentes por aplicativo nem indicador de espaços no shell nesta entrega.

Os limites mínimos são verificados antes de posicionar. Um tile inviável torna-se flutuante, preservando tamanho válido; reinseri-lo exige ação do usuário. A verificação é conservadora para manter custo linear: pode deixar uma janela flutuante mesmo quando outra proporção permitiria encaixá-la. Limites máximos reduzem o conteúdo dentro do tile, deixando espaço livre. Aplicativos que desrespeitam configure e telas menores que o tamanho mínimo do aplicativo ainda podem ultrapassar a área útil.

Pedidos xdg de maximização e tela cheia são separados do layout. Tela cheia usa a saída central e oculta a moldura; sua árvore fica acima dos layers top e abaixo dos overlays. Restaurar retorna ao layout ou à geometria flutuante anterior. Multimonitor por espaço, seleção da saída pedida pelo cliente e regras de aplicativos continuam pendentes.

`JLINUX_TILING=0` no ambiente do compositor mantém organização manual; `1` (padrão) habilita a automática. Outros valores fazem a inicialização falhar. A opção de processo não altera configurações persistidas. Os atalhos de organização e as escolhas por janela duram a sessão. O pacote instalado e o compositor já em execução não são atualizados por compilar esta branch.

## Validação e medição

`meson test -C build-tiling --print-errorlogs` inclui terminais reais em headless/Pixman 1280×720: 1/2/3 janelas, alteração da divisão, promoção, flutuante, maximização/restauração, tela cheia, troca/envio entre espaços, fechamento e recuperação do foco. Os testes antigos de arraste/molduras e navegação em Arquivos usam modo manual para manter seus cenários de regressão.

Isso verifica comportamento, não latência de apresentação nem desempenho de GPU. Não há evidência de ser o gerenciador mais rápido entre alternativas.

Plano PERF-01: comparar release manual (`JLINUX_TILING=0`) e automática (`1`) com 1, 3, 10 e 30 clientes equivalentes, repetindo abertura/fechamento e alteração de divisão. Registrar commit, wlroots, backend, resolução/frequência e alimentação; medir separadamente duração do rearranjo, configure→commit, tempo de quadro p50/p95/p99, CPU em repouso e PSS. Usar headless para custo de CPU e sessão aninhada para apresentação; GPU/energia exigem medição física posterior, preservando a sessão principal e a investigação STABILITY-BOOT. Sanitizers servem apenas a correção e não à comparação de velocidade.

Validação local desta entrega: 16 testes release e 16 com ASan/UBSan passaram em headless/Pixman; `git diff --check` passou. A fixture xdg cobre mínimos/máximos, atualização dos limites, centralização de diálogos e herança de espaço em transientes aninhados. CI remoto, comparação visual e benchmark de hardware ainda não executados no momento do registro.

## Integração na base instalada — feature/window-auto-layout

A implementação de feature/automatic-tiling foi reaproveitada sobre fix/visible-browser-dock, preservando o dock configurável, o navegador fixado e o tema escuro já instalados. Não é merge de PR. PR #38 permanece separado; esta entrega prepara a integração necessária ao pacote atual. As configurações pessoais de diretórios e do dock são preservadas. Instalar o pacote não troca o compositor que já está rodando: o modo automático entra no próximo login. Não reiniciar o processo compositor isoladamente, pois isso encerra a sessão e seus aplicativos.

A bateria desta base contém 19 testes locais, incluindo dock integrado/auxiliar, login, arquivos e limites de tamanho/transientes. Resultado e versão instalada são registrados em docs/validation.md e TASKS.md. Sem benchmark ou nova validação DRM/VT nesta integração.
