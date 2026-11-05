# Arquitetura proposta

A recomendação prioriza consumo em repouso, manutenção e tempo para entregar uma sessão utilizável. A eficiência ainda precisa ser comparada por medições no hardware real.

```text
Aplicativos Wayland / X11 via Xwayland opcional
                         │
Painel JLinux (C + GTK3 + gtk-layer-shell) ── Fuzzel
                         │
                  Sway / wlroots
                         │
          Wayland · Mesa · DRM/KMS · amdgpu
```

## Decisões

- Sway para composição, janelas, monitores e entrada. A identidade e interface próprias começam no shell. Um compositor próprio adicionaria trabalho de protocolos, foco, acessibilidade e compatibilidade antes de benefícios mensuráveis.
- Painel nativo em C/GTK3 e gtk-layer-shell: loop de eventos, sem polling de hardware ou renderização contínua. O relógio acorda a cada 30 segundos. Medir consumo antes de decidir migração de toolkit.
- Mesa/amdgpu para composição acelerada; não forçar Vulkan, clocks ou opções de kernel sem benchmark.
- Escala inteira, fundo sólido e sem efeitos adicionais. Resolução e frequência selecionadas pelo compositor.
- Fuzzel e Foot como ferramentas iniciais. Futuras integrações por D-Bus com NetworkManager, UPower e PipeWire/WirePlumber; reaproveitar serviços existentes.
- Xwayland opcional para aplicativos legados. Portais GTK/wlr entram na integração de sessão.

## Limites do protótipo

Painel com lançador e relógio em um monitor escolhido pelo compositor. Ainda não há bandeja, espaços de trabalho no painel, notificações, áudio/rede, energia, bloqueio de tela ou instalação no gerenciador de login. A configuração é experimental; integrar bloqueio e suspensão antes de usá-la diariamente. Não houve execução gráfica neste ambiente.

## Medição e metas

Medir PSS dos processos da sessão, CPU ociosa por 5 minutos e energia com brilho/frequência fixos. Comparar com a sessão atual sob as mesmas condições. Meta inicial, ainda não demonstrada: menos de 300 MiB de PSS para compositor, painel e lançador, excluindo serviços compartilhados e aplicativos. Ajustar após estabelecer a linha de base.

Medir latência do lançador e fluidez na resolução nativa. Verificar suspensão, retomada, monitores externos e acessibilidade. Reduzir repaints e despertares antes de adicionar efeitos. Manter build x86_64 portátil inicialmente.

## Evolução

1. Compilar e testar em Sway aninhado; estabelecer linha de base.
2. Adicionar espaços de trabalho via IPC, lançador integrado e preferências visuais.
3. Integrar notificações, áudio, rede, energia, portais, acessibilidade e bloqueio.
4. Validar sessão completa, empacotar para Arch e adicionar entrada de login.

## Referências primárias

- https://github.com/swaywm/sway
- https://github.com/wmww/gtk-layer-shell
