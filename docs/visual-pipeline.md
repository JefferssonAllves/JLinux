# Animações, efeitos e renderização

A divisão solicitada é `src/animation/`, `src/effects/` e `src/renderer/`. Esses módulos pertencem à implementação nativa. A inspiração Glass é visual; protótipos web continuam separados da tecnologia de execução do desktop.

| Módulo | Responsabilidade | Não deve controlar |
| --- | --- | --- |
| animation/ | Tempo monotônico, curvas, interpolação, interrupção e movimento reduzido | Shaders, buffers ou ativação de efeitos |
| effects/ | Definição de efeitos, parâmetros, dependências de conteúdo e passes necessários | Relógio, agendamento global ou apresentação |
| renderer/ | Recursos gráficos, execução de passes, sincronização, damage e apresentação | Preferências de estilo, curvas ou política adaptativa |

## Fluxo proposto

```text
Preferências do usuário + capacidades + política de desempenho
                              │
                 Estado visual efetivo por superfície
                              │
            animation/ atualiza propriedades no instante t
                              │
            effects/ descreve os efeitos ativos e seus passes
                              │
            renderer/ executa, apresenta e envia frame-done
```

A cena e o agendador do compositor unem essas etapas. Efeito estático não exige loop de animação. Cena parada não deve ser atualizada continuamente. O toolkit poderá compartilhar a biblioteca matemática de animações, sem compartilhar objetos ou ponteiros entre processos.

## Controles independentes

- Animações desativadas: aplicar o estado final das transições; manter efeitos estáticos escolhidos.
- Blur desativado: remover apenas o passe de desfoque; manter sombra, reflexo, tonalização e demais efeitos habilitados.
- Refração desativada: manter transparência, blur e reflexo conforme suas próprias configurações.
- Todos os efeitos desativados: caminho simples, sem passes de pós-processamento; animações geométricas podem continuar.
- Alteração de um efeito: cancelar apenas canais que dependam desse efeito, recompor uma vez a região afetada e preservar transições independentes.

Os efeitos poderão ser configurados globalmente e, quando implementadas regras por superfície, sobrescritos por componente/janela. Um preset Glass inicializa uma combinação; não deve reaplicar valores e apagar alterações individuais do usuário.

O controle adaptativo resolve o estado efetivo sem sobrescrever a preferência persistida. Só reduz os efeitos autorizados e mostra o motivo. Renderer não toma essa decisão sozinho. Redução de movimento, redução de transparência e aumento de contraste são controles separados.

## Recursos e eficiência

Efeito desativado não deve reservar ou atualizar recursos exclusivos desnecessários. Recursos compartilhados permanecem enquanto usados; destruição deve respeitar buffers e trabalho GPU em voo. Blur e sombras expandem a região de damage; refração precisa considerar o conteúdo amostrado. Tamanhos de kernel e caches exigem limites e medição.

Um efeito que amostra o fundo pode exigir composição e impedir direct scanout naquele quadro. O renderer deve testar elegibilidade e usar fallback correto; não prometer scanout direto com toda combinação de Glass. Medir separadamente o custo dos efeitos e o custo de animar seus parâmetros.

## Estado real

Nesta entrega, renderer.c passou a possuir a criação/limpeza do renderizador e alocador wlroots e a apresentação/frame-done, extraídas de compositor/main.c. O comportamento gráfico existente foi preservado.

animation/ e effects/ contêm fronteiras documentadas, sem motor, shaders ou API de passes implementados. O TOML continua sendo exemplo não carregado. Não há toggles funcionais, recarga ao vivo ou efeito Glass disponível ainda.
