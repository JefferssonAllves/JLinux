# Plugins e skills para JLinux — pesquisa de 06/11/2026

**Resultado:** priorizar um piloto pequeno com RTK OSS para saídas de terminal, Serena para consultas de código C e Impeccable para revisão dos protótipos. Playwright CLI é complementar para protótipos web. Para o visual definitivo, a prioridade técnica é capturar e comparar o JLinux nativo com as referências do usuário; nenhuma dessas ferramentas substitui esse teste.

Pesquisa concluída; **nenhuma nova skill, plugin, hook, serviço ou dependência foi instalada**. Não foram alterados modelos, permissões, configuração global, sessão Wayland, PAM ou preferências visuais. Recomendações abaixo são julgamento técnico, não integração validada ou economia medida no projeto.

## Escopo e evidência

- 40 consultas de repositórios no GitHub: 37 resolvidas e três não encontradas. Renomeações foram acompanhadas pela API.
- Trechos de README e cabeçalhos/condições de licença de 20 repositórios examinados; sete arquivos de skills/adaptações examinados em commits fixados. Os demais receberam triagem por metadados e documentação pública, sem auditoria completa.
- Seis referências em vídeo: trechos de uma transcrição do publicador, material do autor e descrições/capítulos. O acesso direto ao YouTube sofreu throttling; não houve reprodução integral de todos os vídeos. A tentativa de baixar o JSON da transcrição diretamente recebeu 403; trechos ficaram acessíveis pela ferramenta de pesquisa.
- Inventário do ambiente e catálogo de plugins conferidos. Arquivos externos foram tratados como material de análise; suas instruções de instalar, conceder confiança a hooks ou iniciar agentes não foram executadas.
- [Inventário verificável](skills-plugins-2026-11-06.json): URLs, atividade, estrelas, licenças declaradas, hashes dos arquivos lidos e commits dos sete arquivos principais. Estrelas/instalações são sinais de adoção, não prova de segurança ou qualidade. Dados podem mudar.

A avaliação considera o estado de trabalho do JLinux, com compositor/shell em C, wlroots 0.20, renderer/effects/animation separados e navegador WPE. A branch documental parte de `develop`; as funcionalidades das branches de trabalho não foram integradas por esta pesquisa.

## Seleção para um primeiro piloto

| Ferramenta | Benefício esperado no JLinux | Limite e condição de adoção |
| --- | --- | --- |
| [RTK OSS](https://github.com/rtk-ai/rtk) | Compactar saídas repetitivas de Git, busca e testes antes de chegarem ao modelo. | Começar por invocação manual em comandos de leitura. Confirmar suporte a Meson/Ninja, códigos de saída e preservação de erros. Não ativar reescrita global de comandos como `git push`. |
| [Serena](https://github.com/oraios/serena) | Buscar símbolos/referências e ler funções específicas em C em vez de arquivos inteiros. | C/C++ é suportado pelo backend LSP; exige configurar clangd e o `compile_commands.json` da worktree correta. Só compensa se reduzir releituras sem perder referências. |
| [Impeccable](https://github.com/pbakaus/impeccable) | Vocabulário de crítica, hierarquia, tipografia e refinamento, com instruções por tarefa. | Usar primeiro nos protótipos e como referência para revisão. Não presumir que detectores web validam Cairo/Wayland. A aparência das fotos e as escolhas do usuário prevalecem sobre conselhos genéricos. |
| [Playwright CLI + skill](https://github.com/microsoft/playwright-cli) | Capturar e verificar protótipos web com comandos de escopo pequeno. | O autor propõe menor contexto que MCP para certos fluxos; não medimos isso aqui. Não controla a barra nativa WPE nem testa o compositor próprio. |
| [UI UX Pro Max](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill) | Consultar localmente regras de espaçamento, contraste, tipografia e estados de interação. | Alternativa ao Impeccable para consulta; evitar ativar os dois em toda tarefa. As stacks publicadas não incluem o toolkit C próprio; não aceitar fallback automático para HTML/Tailwind no desktop. |
| [Designer Skills — design-tokens](https://github.com/julianoczkowski/designer-skills/tree/main/design-tokens) | Organizar tokens semânticos para comunicar decisões visuais sem repetir descrições longas. | A saída é orientada a CSS/JSON e precisa de adaptação explícita ao pipeline C. Não criar uma segunda paleta ou substituir as referências aprovadas. |

**Ordem sugerida:** medir a situação atual; testar RTK OSS e Serena separadamente; escolher apenas uma skill principal de design para protótipos; verificar capturas do shell nativo antes de ampliar o conjunto. Trata-se de uma proposta, não de instalação autorizada ou executada por este relatório.

Adoção observada no catálogo skills.sh: cerca de 387 mil instalações de Impeccable e 436 mil de UI UX Pro Max. [Impeccable](https://www.skills.sh/pbakaus/impeccable/impeccable), [UI UX Pro Max](https://www.skills.sh/nextlevelbuilder/ui-ux-pro-max-skill/ui-ux-pro-max). São números aproximados de páginas com caches distintos; não equivalem a testes de compatibilidade ou auditoria. A instalação da coleção inteira não é necessária.

## Pontos que mudam a decisão

**RTK:** a documentação atual distingue `rtk-oss`, Apache-2.0, de `rtk` completo, que também inclui BUSL-1.1. A licença Apache informada nos metadados do repositório não descreve sozinha todas as edições. O [adaptador Codex inspecionado](https://github.com/rtk-ai/rtk/blob/1cf7bc703efa5cd0530222746ce40bd5d9b38336/hooks/codex/README.md) documenta reescrita via hooks e limitações de classificação de comandos. Esta sessão usa `functions.exec`/`exec_command`; não foi demonstrado que o hook que corresponde a `Bash` intercepta esse caminho. Prefixo manual e comparação com a saída original são o primeiro teste proposto.

**Serena:** a aplicação é GPL-3.0-or-later; SolidLSP é MIT. Essa divisão consta no arquivo de licença atual. Uso como ferramenta de desenvolvimento não exige incorporar o servidor no desktop. Não está instalado nem foi indexado este projeto. Índices precisam acompanhar a worktree/commit; resultados de uma branch não podem ser tratados como evidência de outra.

**Impeccable:** o arquivo examinado tem 11.762 bytes e carrega referências específicas, contexto e um launcher. Partes do fluxo podem usar hooks, baixar um binário e solicitar agentes; não é apenas um pequeno prompt. O escopo do piloto deve preservar a execução direta por padrão do JLinux. A entrada antiga `frontend-design` do mesmo projeto aparece como renomeada/depreciada; instalar um nome antigo de vídeo pode não instalar o fluxo atual. [Catálogo atual](https://www.skills.sh/pbakaus/impeccable/impeccable).

**Context Mode:** há adaptação Codex real no repositório, mas a licença atual é **Elastic License 2.0**, source-available, não MIT. O instalador descrito também modifica configuração e confiança de hooks. Sua alegação de redução de bytes de resultados não demonstra redução da conta total. Fica como piloto posterior, isolado, após verificar cobertura do cliente, retenção de dados e retorno dos logs completos. [Instruções inspecionadas](https://github.com/mksglu/context-mode/blob/281bbd960bf1a14e7a60cbf4c3e3c6b487f7f7e0/docs/install/codex.md).

**Context7:** há um [índice público de wlroots](https://context7.com/websites/wlroots_pages_freedesktop_wlroots), com atualização antiga indicada no catálogo. A busca indexada exibiu erro 404 em uma resposta, mas a abertura posterior da página retornou seus metadados. Não foi feita consulta de documentação pelo MCP; a cobertura operacional da série 0.20 continua não verificada. Headers locais e fontes oficiais continuam sendo a referência para C/Wayland. O plugin foi encontrado no catálogo, sem instalação.

## Catálogo amplo e decisão de triagem

“Considerar” significa avaliar em piloto; não significa compatibilidade comprovada. Licença abaixo é a declaração observada; pacotes e edições podem ter termos próprios. A profundidade da leitura está no JSON.

| Repositório | Tipo / licença observada | Decisão para o projeto |
| --- | --- | --- |
| [rtk-ai/rtk](https://github.com/rtk-ai/rtk) | CLI; OSS Apache-2.0, edição completa também BUSL-1.1 | Primeiro piloto de compactação, manual. |
| [oraios/serena](https://github.com/oraios/serena) | MCP/LSP; GPL-3.0-or-later + componente MIT | Primeiro piloto de recuperação de código C. |
| [pbakaus/impeccable](https://github.com/pbakaus/impeccable) | Skill/harness; Apache-2.0 | Candidato principal de revisão de protótipos. |
| [nextlevelbuilder/ui-ux-pro-max-skill](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill) | Skills/base consultável; MIT | Alternativa para consulta visual, sem adotar stack web no shell. |
| [julianoczkowski/designer-skills](https://github.com/julianoczkowski/designer-skills) | Skills; Apache-2.0 | Selecionar tokens/brief; a revisão web não serve diretamente ao shell C. |
| [microsoft/playwright-cli](https://github.com/microsoft/playwright-cli) | CLI/skill; Apache-2.0 | Preferência de piloto para verificação de protótipos. |
| [microsoft/playwright-mcp](https://github.com/microsoft/playwright-mcp) | MCP; Apache-2.0 | Alternativa ao CLI quando a inspeção persistente do browser for necessária. |
| [ChromeDevTools/chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp) | MCP; Apache-2.0 | Útil a páginas Chromium; não presumir compatibilidade com WPE. |
| [figma/mcp-server-guide](https://github.com/figma/mcp-server-guide) | Guia da integração oficial; licença não declarada na API | Considerar se houver trabalho de desenho editável em Figma. Requer conta/conexão. |
| [tokens-studio/figma-plugin](https://github.com/tokens-studio/figma-plugin) | Plugin Figma; MIT declarada | Opcional se Figma virar origem dos tokens; verificar recursos do plano. |
| [style-dictionary/style-dictionary](https://github.com/style-dictionary/style-dictionary) | Ferramenta de build; Apache-2.0 | Auxiliar opcional; saída C precisaria de formatter/adaptação, não foi implementada. |
| [21st-dev/magic-mcp](https://github.com/21st-dev/magic-mcp) | MCP; ISC | Já existe 21st na sessão; usar para protótipos, sem reinstalar por outro nome. |
| [anthropics/skills](https://github.com/anthropics/skills) | Catálogo; conferir licença por skill | `frontend-design` já disponível localmente; evitar cópias redundantes. |
| [openai/skills](https://github.com/openai/skills) | Catálogo Codex; conferir licença por skill | Fonte para skills específicas; GitHub/Docs já cobertos no ambiente. |
| [trailofbits/skills](https://github.com/trailofbits/skills) | Skills de segurança; CC-BY-SA-4.0 declarada na versão atual | ASan, fuzzing e revisão já instalados com versão anterior registrada. Não atualizar silenciosamente. |
| [mksglu/context-mode](https://github.com/mksglu/context-mode) | Plugin/MCP; ELv2 | Segunda etapa, sem hooks globais antes de avaliação. |
| [tobi/qmd](https://github.com/tobi/qmd) | Busca local; MIT | Adiar enquanto `rg` resolver. BM25 pode funcionar sem LLM; busca semântica inclui modelos locais. |
| [thedotmack/claude-mem](https://github.com/thedotmack/claude-mem) | Memória; Apache-2.0 declarada | Adiar: armazenamento, observer e opções de provedor adicionam custo e tratamento de dados. |
| [obra/superpowers](https://github.com/obra/superpowers) | Framework de skills; MIT | Selecionar métodos pontuais; não impor pipeline, aprovações ou agentes em toda alteração. |
| [affaan-m/ECC](https://github.com/affaan-m/ECC) | Catálogo/harness; MIT | Referência de organização; instalação em massa não priorizada. |
| [code-yeongyu/oh-my-openagent](https://github.com/code-yeongyu/oh-my-openagent) | Orquestração; licença não identificada pela API | Não priorizar: outro fluxo de agente, sem benefício local demonstrado. |
| [msitarzewski/agency-agents](https://github.com/msitarzewski/agency-agents) | Perfis; MIT | Redundante aos perfis especializados existentes; mais perfis não provam economia. |
| [Fission-AI/OpenSpec](https://github.com/Fission-AI/OpenSpec) | Especificação; MIT | Referência para tarefas grandes; evitar competir com TASKS.md e docs do projeto. |
| [github/spec-kit](https://github.com/github/spec-kit) | Toolkit de especificação; MIT | Mesma ressalva; novo processo só se diminuir retrabalho medido. |
| [github/github-mcp-server](https://github.com/github/github-mcp-server) | MCP oficial; MIT | `gh` já resolve repositórios/PRs/CI; conectar somente se trouxer capacidade necessária. |
| [modelcontextprotocol/servers](https://github.com/modelcontextprotocol/servers) | Servidores de referência; verificar licença por componente | Não instalar filesystem/git/shell duplicados sem necessidade. |
| [PrefectHQ/fastmcp](https://github.com/PrefectHQ/fastmcp) | SDK; Apache-2.0 | Só se precisarmos criar integração própria de desenvolvimento. Não é plugin pronto de design. |
| [Aider-AI/aider](https://github.com/Aider-AI/aider) | Outro agente/CLI; Apache-2.0 | Repo map é referência; não trocar de cliente para testar a hipótese de recuperação. |
| [benjitaylor/agentation](https://github.com/benjitaylor/agentation) | Feedback em UI web; PolyForm Shield 1.0.0 | Considerar só em protótipos; depende da página e tem licença distinta de MIT. |
| [TimLai666/wayhand-mcp](https://github.com/TimLai666/wayhand-mcp) | Automação desktop; MIT | Experimental, pouca adoção. Backend sandbox é caminho de estudo; não executar setup no desktop ativo. |
| [kurojs/wayland-mcp](https://github.com/kurojs/wayland-mcp) | Automação desktop; GPL-3.0 declarada | Experimental; exige backends de captura/entrada disponíveis no compositor. |
| [Zetakai/desktop-mcp](https://github.com/Zetakai/desktop-mcp) | Automação desktop; MIT | Experimental; demonstrações em COSMIC não validam JLinux próprio. |
| [EftikharAzim/youtube-to-skill](https://github.com/EftikharAzim/youtube-to-skill) | Conversão de vídeo; MIT no arquivo, não identificada pela API | Não priorizar: pouca adoção; converter transcrição em instrução não valida a recomendação. |
| [karpathy/autoresearch](https://github.com/karpathy/autoresearch) | Experimentos em treinamento GPU; licença não declarada pela API | Inspiração de medição; não é otimizador pronto do compositor e pode ampliar chamadas. |
| [anthropics/claude-code](https://github.com/anthropics/claude-code) | Cliente/harness; conferir termos próprios | Referência para entender recomendações dos vídeos; não é instalação de skill Codex. |
| [pbakaus/impeccable-talks](https://github.com/pbakaus/impeccable-talks) | Material de palestras; direitos reservados pelo autor | Evidência do autor e exemplos de processo, não dependência do produto. |
| [upstash/context7](https://github.com/upstash/context7) | Documentação/MCP; MIT declarada | Consulta eventual, com versão e fonte verificadas; cobertura de wlroots não demonstrada operacionalmente. |

Consultas não resolvidas: `googlechrome/chrome-devtools-mcp` foi corrigida para `ChromeDevTools/chrome-devtools-mcp`; `AbanteAI/sidequest` e `quantgeekdev/lean-spec` não foram recomendadas. URLs antigas de Oh My OpenCode, Everything Claude Code, FastMCP e Style Dictionary redirecionaram para os nomes da tabela.

## Recomendações em vídeo

| Vídeo | O que foi possível examinar | Aplicação e ressalva |
| --- | --- | --- |
| [The Dark Arts of Skill Engineering — Paul Bakaus](https://www.youtube.com/watch?v=SQMCtZX3trg) | Trechos da [transcrição e capítulos do publicador](https://ai.engineer/talks/SQMCtZX3trg-dark-arts-skill-engineering-paul-bakaus), incluindo a seção de 20:30 sobre carregar regras por tarefa. | Contexto seletivo e resultados reutilizáveis são úteis. Críticos/agentes extras são outro mecanismo e podem aumentar o total de tokens; não foram ativados. |
| [Design at the Speed of Adjectives — Paul Bakaus](https://www.youtube.com/watch?v=v42opQpCy60) | Material do [repositório de palestras do autor](https://github.com/pbakaus/impeccable-talks), sem reprodução integral. | Linguagem visual precisa deve partir das fotos do usuário, sem gerar uma paleta diferente a cada tela. |
| [Claude Code Skills for Designers — AI for Work](https://www.youtube.com/watch?v=1pV7bvbaCFg) | Descrição, capítulos e [repositório ligado pelo autor](https://github.com/julianoczkowski/designer-skills). | Brief → tokens → revisão com captura. O fluxo demonstrado é web; não comprova testes do toolkit C. |
| [Skills em todos os projetos — Matheus Battisti / Hora de Codar](https://www.youtube.com/watch?v=ZQBcUBsN2k8) | Descrição/capítulos publicados sobre Superpowers, Frontend Design e Autoresearch. | Boa referência de processo em português; aplicar só os passos necessários ao JLinux, sem adotar o pipeline inteiro. |
| [RTK: Up To 90% Token Savings For Claude Code And Codex](https://www.youtube.com/watch?v=5p0f2p9Gn0o) | Descrição indexada; reprodução direta limitada. | A porcentagem é alegação do vídeo. Conferimos a edição/licença e o adaptador atuais no GitHub; não medimos essa economia. |
| [Introduction to Serena in 5 Minutes](https://www.youtube.com/watch?v=5QN7gN1KYLA) | Link indicado pelo README oficial; não foi obtida transcrição integral. | Referência introdutória, com suporte C/C++ conferido na documentação do projeto, não no vídeo. |

Não foram copiados vídeos/transcrições para o repositório. Vídeos serviram para encontrar métodos e candidatos; documentação e arquivos dos projetos sustentam as decisões técnicas.

## Economizar tokens sem prejudicar a qualidade

A [documentação oficial](https://learn.chatgpt.com/docs/build-skills) descreve carregamento progressivo: nomes e descrições entram primeiro; o corpo é lido quando a skill é usada. Portanto, tamanho em disco não equivale a tokens pagos em todo turno. APIs com [tool search](https://developers.openai.com/api/docs/guides/tools-tool-search) também podem carregar ferramentas sob demanda; isso não é uma configuração universal que possamos simplesmente ligar em qualquer cliente.

No ambiente, os corpos das três skills do projeto somam 3.643 bytes. Exemplos de corpos de skills do usuário: Graphify 49.528 bytes, libFuzzer 24.234, frontend-design 9.390. São **bytes UTF-8, não contagem de tokens**; referências podem acrescentar contexto quando lidas. Há atalhos de descoberta em `~/.agents/skills` para `~/.codex/skills`; não comprovamos carregamento duplicado nem os removemos.

Ações propostas:

1. Manter o conteúdo carregado por tarefa pequeno: ler cabeçalhos/trechos necessários, não catálogos inteiros. As regras de C/Wayland e os critérios de teste continuam obrigatórios.
2. Salvar logs completos como artefatos e retornar resumo mais falhas ao agente. Um filtro não pode esconder exit code, stack trace ou caminho de recuperação do original.
3. Usar `rg` para buscas simples. Graphify já tem fallback para isso quando não há grafo; usar grafo/Serena em perguntas estruturais que justifiquem o índice.
4. Registrar decisões visuais aprovadas, tokens e capturas de referência com o commit correspondente. Evitar redescobrir a estética em cada tarefa ou ler resultados de outra branch.
5. Acionar uma skill de design por objetivo e referências específicas. Não ativar todos os guias, MCPs, scanners e revisores a cada mudança.
6. Não introduzir agentes permanentes ou modelos fixos por instrução de pacote externo. Medir custo total, incluindo buscas, compilação/indexação, tentativas e agentes adicionais.

O conjunto existente já inclui Ponytail, frontend-design, 21st, Liquid Glass e as três skills JLinux. ASan/fuzzing/revisão têm utilidade específica. Nenhuma reinstalação em massa foi proposta.

## A lacuna para melhorar o design nativo

Playwright e Chrome DevTools inspecionam páginas; não estabelecem que a janela WPE, dock, moldura ou compositor C estejam corretos. A inspeção do compositor na worktree ativa não encontrou implementação de screencopy/image-copy-capture/portal por esses nomes; isso não é uma auditoria de todos os protocolos ou branches.

Antes de conectar automação de desktop, uma tarefa futura deve fornecer captura reproduzível do JLinux em execução headless/aninhada, comparar na mesma resolução com as duas fotos principais e verificar cliques/foco separadamente. Os MCPs Wayland encontrados dependem de backends e, em alguns modos, de acesso ao desktop real. Não foram usados para controlar a sessão atual nem para conceder acesso a dispositivos de entrada.

A skill `jlinux-visual` pode ganhar uma referência curta para essa comparação e para os critérios de aceite. Isso é proposta de adaptação local, ainda não implementada. Python/Node em ferramentas de desenvolvimento não autorizam adicioná-los à inicialização ou ao caminho gráfico do produto.

## Como decidir a adoção

Piloto futuro com as mesmas tarefas/commit/modelo, sem mudar mais de uma variável:

| Piloto | Comparação | Aprovar somente se |
| --- | --- | --- |
| RTK OSS | Git read-only, busca e Meson/Ninja com saída original versus filtrada | Menos contexto com a mesma detecção de erro e acesso ao log completo; contadores do RTK não são tratados como fatura. |
| Serena | Localizar listeners, referências e função C usando `rg` versus LSP | Correção equivalente, menos leitura repetida e custo de indexação compensado. |
| Skill de design | Um componente com guia atual versus skill adicional | Aproximação às fotos melhora, personalização é preservada e diminui o retrabalho total. |
| Playwright CLI | Mesma verificação de protótipo por CLI versus MCP | Mesmo comportamento/capturas, menor contexto ou menos chamadas. |

Usar métricas fornecidas pelo próprio cliente para input/output/cache quando disponíveis; registrar separadamente bytes de ferramentas se não houver contador de tokens confiável. Medir tempo e sucesso da tarefa, não apenas tamanho da primeira resposta. Não há neste relatório benchmark local desses candidatos, análise completa de segurança ou prova de economia percentual.

## Estado da entrega

Pesquisa e triagem concluídas. Foram produzidos relatório e inventário, com checagem de JSON, links locais e whitespace. Não houve alteração no binário/instalação do JLinux, instalação de plugins, modificação de permissões, benchmark ou teste visual físico. Instalações e pilotos permanecem trabalho futuro, não requisitos pendentes desta pesquisa.
