# Arquitetura do ambiente gráfico próprio JLinux

## Requisitos centrais

O JLinux terá compositor Wayland e ambiente gráfico próprios. Aparência, animações, cores, funcionalidades, atalhos e comportamento das janelas devem ser personalizáveis conforme o gosto do usuário. O sistema deve se adaptar ao hardware e à carga para buscar o máximo desempenho possível dentro das escolhas visuais do usuário.

Usaremos bibliotecas de infraestrutura: wlroots 0.20 para backends e primitivas Wayland, libwayland para transporte, Mesa/amdgpu para GPU e libinput/xkbcommon para entrada. O executável, as políticas de janelas, foco, espaços de trabalho, efeitos, animações, configuração e integração do shell serão do JLinux. Não há dependência de um compositor Sway instalado. Desenvolver um compositor próprio não exige reimplementar drivers ou o protocolo de transporte.

```text
Aplicativos Wayland                 Shell e configurações JLinux
                \                   /
                  jlinux-compositor
        configuração · janelas · foco · animações
             cena · efeitos · controle de desempenho
                           │
           wlroots · libwayland · libinput · xkbcommon
                           │
                Mesa · DRM/KMS · amdgpu
```

## Protótipos e implementação definitiva

Ferramentas web foram solicitadas para criar protótipos de tela e validar a direção visual com o usuário antes de construir a interface definitiva. shadcn, Magic/21st e skills de frontend/Liquid Glass são recursos dessa fase de exploração. Não estabelecem dependências de execução do desktop.

O fluxo é prototipar a tela, iterar o visual com o usuário e implementar o resultado no ambiente nativo. O shell GTK/CSS foi substituído por um cliente Wayland/C com componentes próprios. O toolkit é inicial e usa Cairo/Pango para desenho/tipografia; o material inicial do painel usa blur CPU do wallpaper em cache, tonalização e reflexo de borda. Fidelidade completa e efeitos GPU sobre janelas ainda não estão implementados.

A escolha técnica deve priorizar as tecnologias mais adequadas e eficientes para cada função, com comparação no hardware real. Avaliar latência, tempo de quadro, CPU/GPU, memória, energia, robustez e custo de manutenção. A linguagem ou biblioteca isolada não prova desempenho. A base atual é C/wlroots/Wayland/Mesa; mudanças dessa base precisam de justificativa técnica, sem adoção automática de uma stack por disponibilidade de ferramentas de prototipação.

## Personalização como parte da arquitetura

- Configuração declarativa versionada, com temas de cores, fontes, bordas, formas, transparência, sombras e desfoque.
- Animações configuráveis por evento: abrir, fechar, minimizar, trocar espaço de trabalho. Duração, curva e ativação independentes; preferência por movimento reduzido.
- Políticas de janelas configuráveis: flutuante ou mosaico, regras por aplicativo, foco, atalhos, gestos e espaços de trabalho.
- Shell com componentes habilitáveis: painel, lançador, notificações e controles de sistema. Evoluir os componentes sob identidade JLinux, reduzindo dependências de ferramentas externas.
- Alterações ao vivo: validar uma nova configuração inteira antes de aplicá-la, preservar a configuração anterior em caso de erro e permitir desfazer na interface de preferências.
- Configuração do sistema, tema e preferências do usuário separados; preferência explícita do usuário tem precedência. Funcionalidades novas continuam exigindo implementação ou extensões suportadas.

`config/jlinux/desktop.example.toml` é uma proposta de contrato; o compositor atual ainda não lê esse arquivo. A primeira interface de preferências deve consumir o mesmo modelo do arquivo, sem uma segunda fonte de verdade.

## Adaptação ao hardware e à carga

No Ryzen 7 5700U com Radeon integrada e 7,1 GiB de RAM utilizável, priorizar memória e largura de banda da GPU. A capacidade de CPU permite desenvolvimento local, mas a cena e os recursos gráficos devem permanecer econômicos.

1. Detectar backend, renderizador, formatos e capacidades reais das saídas; escolher recursos por capacidade, sem depender apenas do nome da GPU.
2. Medir tempo de apresentação e renderização por saída. Derivar orçamento da frequência real: 60 Hz oferece aproximadamente 16,67 ms por quadro; o orçamento precisa deixar margem para clientes e composição. Essa frequência é exemplo, não uma medição da tela local.
3. Renderizar sob demanda, rastrear regiões alteradas e suspender animações invisíveis. Usar tempo monotônico e progressão independente da taxa de quadros.
4. Manter texturas e buffers com limites; evitar alocações a cada quadro. Investigar direct scanout quando elegível e não houver efeitos que o impeçam.
5. No modo adaptativo, reduzir primeiro custo de blur e sombras, conforme autorização na configuração. Só reduzir animações se permitido. Usar médias, histerese e recuperação gradual para evitar oscilações de qualidade.
6. Considerar alimentação, bateria e pressão térmica quando disponíveis. A coleta deve ser orientada a eventos ou amostragem limitada, sem polling acelerado.
7. Mostrar qualidade efetiva e motivo dos ajustes. O modo fixo preserva preferências; o modo adaptativo respeita os limites escolhidos. Não mudar resolução, escala ou frequência sem autorização explícita na configuração.

Desempenho máximo e efeitos máximos podem disputar o mesmo orçamento. O usuário controla esse compromisso. Não prometer FPS, economia ou consumo antes de comparar resultados no hardware real.

## Pipeline visual separado

A estrutura é `src/animation/` para tempo/curvas, `src/effects/` para efeitos independentes e `src/renderer/` para recursos gráficos e apresentação. Contratos e estado real em [visual-pipeline.md](visual-pipeline.md). A direção visual é inspirada no Glass dos dispositivos Apple, conforme [visual-style.md](visual-style.md), preservando identidade e preferências próprias.

O módulo renderer já contém criação/limpeza do renderizador/alocador e apresentação da cena. animation/ ainda documenta o motor planejado. effects/glass.c já compõe o material do shell e das molduras; shaders e passes de blur sobre janelas não foram implementados. Cada efeito deverá poder ser ativado ou desativado independentemente do motor de animações.

## Organização progressiva

O primeiro código está em `src/compositor/main.c`. Conforme as responsabilidades forem implementadas, dividir em módulos de backend/saídas, entrada, janelas, cena, animações, efeitos, configuração e telemetria. Evitar módulos vazios ou APIs prematuras.

Manter política de janelas e interpretação de preferências fora dos detalhes de backend. Integrar o shell por protocolos padronizados quando disponíveis; um futuro IPC local deve ter permissões e versão explícitas. Não carregar código arbitrário de temas dentro do compositor. Extensões executáveis devem preferir processos separados.

## Estado implementado

`jlinux-compositor` mantém servidor, saídas, janelas e entrada. O renderer wlroots é separado em src/renderer/. `jlinux-shell` agora é um cliente libwayland-client em C que implementa diretamente layer-shell, buffers wl_shm limitados por superfície, ponteiro, painel. O toolkit inicial em src/toolkit/ fornece desenho de superfícies, ícones, texto e botões via Cairo/Pango; GTK e CSS saíram do runtime.

O iniciador jlinux-session também é C: runtime privado, prontidão por pipe, supervisão por signalfd/timerfd e limpeza dos grupos criados. Python é usado nos testes, não na sessão.

O shell redesenha por alterações e relógio de 30 segundos. O compositor calcula a área útil dos layers; novas janelas evitam barra superior e posições encobertas são ajustadas quando essa área muda. Essa política inicial usa a saída central; não constitui gerenciamento multimonitor completo.

Super+Enter abre o terminal; aplicativos são iniciados por comando. O botão de energia abre Sair da sessão, Reiniciar e Desligar com confirmação. As solicitações usam loginctl/systemctl sem sudo, sem forçar bloqueios e com erro visível em caso de recusa. O botão Som controla o volume da saída padrão via wpctl/WirePlumber (±5%, limite de 100%, mute e consulta de estado). As solicitações são assíncronas com timeout de 3 segundos; falhas mostram indisponibilidade. Wi-Fi, Bluetooth e demais menus não estão integrados: o shell mostra aviso em vez de inventar estados ou executar ações. Há molduras nativas com material compartilhado, transparência, título/fechar e arraste/resize; o blur permanece restrito ao wallpaper do shell. Não há leitura TOML, recarga de tema, blur de janelas/refração, animações, IME ou bloqueio. Popups xdg têm suporte inicial; acessibilidade do toolkit, escala fracionária e navegação completa continuam pendentes.

O código nativo é a primeira fundação de execução, não uma reprodução completa das referências. Desenho SHM atual é feito em CPU e composição pode ser acelerada pela GPU; eficiência precisa de baseline e comparação real.

## Etapas e critérios de entrega

1. **Fundação:** backend, saídas, socket e encerramento limpo. Validar headless no CI e depois aninhado com GPU local.
2. **Janelas e entrada:** xdg-shell, teclado, ponteiro, foco, mover/redimensionar, popups e atalhos. Validar com múltiplos clientes reais, desconexões e hotplug.
3. **Configuração e identidade:** parser validado, aplicação atômica, temas, regras e política de janelas. Testar configurações inválidas e recuperação.
4. **Animações e efeitos:** agendamento por quadro, durações/curvas, sombras, transparência e blur configuráveis. Medir GPU, memória e apresentação antes/depois.
5. **Adaptação:** controle com limites do usuário, métricas por saída, histerese e indicador de qualidade. Comparar modo fixo/adaptativo com carga reproduzível e na bateria.
6. **Ambiente completo:** shell próprio, portais, áudio, rede, energia, acessibilidade, bloqueio e suspensão. Validar retomada e monitores externos antes da adoção diária.
7. **Distribuição:** pacote Arch, sessão de login e Xwayland opcional.

Metas iniciais: estabelecer PSS da sessão, CPU ociosa em 5 minutos, energia sob condições fixas e percentis do tempo de quadro. A antiga meta de 300 MiB é hipótese a reavaliar; não é resultado. Testes de Pixman headless verificam ciclo de vida, não desempenho da GPU nem correção visual.

## Referências primárias

- https://wayland.freedesktop.org/architecture.html
- https://gitlab.freedesktop.org/wlroots/wlroots

## Login e instalação do Arch

`jlinux-greeter` é outro cliente Wayland em C, usando Cairo/Pango/XKB e o material compartilhado. Não é o compositor nem o shell. Fala com greetd por socket UNIX/JSON; PAM permanece responsável pela autenticação. Cage isola a tela de login, e termina antes da sessão JLinux, que usa nosso compositor. A configuração de boot não usa os modos `--demo`/`--preview`.

O pacote pacman instala os binários e uma entrada Wayland. Ativação do greetd é separada, com backup e TTY2. Contratos, recuperação e limites em docs/arch-installation.md. Inicialização completa/PAM físico devem ser relatados separadamente dos testes simulados.

O dock lateral e sua lista/configuração de aplicativos foram removidos por pedido do usuário. O shell não enumera entradas .desktop nem depende de GIO; GLib permanece para o controle de áudio. A região útil ocupa toda a largura, respeitando apenas a reserva superior.
