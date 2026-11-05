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

## Personalização como parte da arquitetura

- Configuração declarativa versionada, com temas de cores, fontes, bordas, formas, transparência, sombras e desfoque.
- Animações configuráveis por evento: abrir, fechar, minimizar, trocar espaço de trabalho. Duração, curva e ativação independentes; preferência por movimento reduzido.
- Políticas de janelas configuráveis: flutuante ou mosaico, regras por aplicativo, foco, atalhos, gestos e espaços de trabalho.
- Shell com componentes habilitáveis: painel, dock, lançador, notificações e controles de sistema. Evoluir os componentes sob identidade JLinux, reduzindo dependências de ferramentas externas.
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

## Organização progressiva

O primeiro código está em `src/compositor/main.c`. Conforme as responsabilidades forem implementadas, dividir em módulos de backend/saídas, entrada, janelas, cena, animações, efeitos, configuração e telemetria. Evitar módulos vazios ou APIs prematuras.

Manter política de janelas e interpretação de preferências fora dos detalhes de backend. Integrar o shell por protocolos padronizados quando disponíveis; um futuro IPC local deve ter permissões e versão explícitas. Não carregar código arbitrário de temas dentro do compositor. Extensões executáveis devem preferir processos separados.

## Estado implementado

`jlinux-compositor` inicializa servidor Wayland, backend, renderizador, alocador, cena e saídas. Publica socket, apresenta fundo sólido, organiza superfícies layer-shell por camada e encerra com SIGINT/SIGTERM. `jlinux-shell` é um cliente GTK separado com wallpaper, mensagem de boas-vindas, barra superior e relógio. O shell aceita CSS adicional via JLINUX_THEME; ainda não lê o TOML.

O teste headless/Pixman mapeia wallpaper e painel e reinicia o shell duas vezes sem derrubar o compositor. Há build ASan/UBSan separado. Não houve validação visual em monitor físico ou medição de desempenho.

A versão 0.4 adiciona janelas xdg, popups básicos, teclado XKB, ponteiro com cursor, foco por clique, alternância, movimento e redimensionamento via Super+mouse. O terminal inicial é Foot, com shell normal ou Codex CLI. O módulo interaction.c contém essas responsabilidades, separado do shell GTK.

Há atalhos para terminal, fechar janela e encerrar sessão. O runner suporta DRM explícito a partir do TTY, supervisiona processos com pidfd e usa grupos de processos para limpeza. Não substitui a sessão de login.

Entrada virtual é habilitada apenas por variável de teste em runtime privado. Popups layer-shell, política completa de área exclusiva para janelas, múltiplas saídas/hotplug avançado, IME, maximização e tela cheia ainda precisam de trabalho. Não há animações, efeitos ou adaptação automática. O primeiro shell usa GTK como etapa de prototipação; toolkit próprio permanece planejado.

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
