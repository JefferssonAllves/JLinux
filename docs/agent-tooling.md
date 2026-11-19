# Ferramentas e perfis de desenvolvimento

O coordenador Codex entrega e valida o trabalho. Perfis especializados são configurações reutilizáveis, não trabalhadores permanentes nem revisão humana independente. Nenhum modelo é fixado: herdam o modelo da sessão. A configuração do projeto limita subagentes abertos a um, além do coordenador, quando houver delegação autorizada. Não altera permissões ou a configuração global.

| Perfil | Responsabilidade |
| --- | --- |
| compositor-engineer | C, Wayland, saídas, entrada e ciclo de vida |
| visual-engineer | Shell, temas, componentes e animações |
| performance-reviewer | Evidências de memória, fluidez e energia; análise por padrão |
| correctness-reviewer | Memória, protocolo e regressões; análise por padrão |

Arquivos: `.codex/agents/*.toml`. O carregamento depende do suporte e confiança do cliente Codex no projeto. A sintaxe foi validada; a descoberta dos perfis precisa ser confirmada no cliente após recarregar. Não foi iniciada uma equipe ou campanha autônoma.

## Skills locais

Versionadas em `.agents/skills/`: `jlinux-compositor`, `jlinux-performance` e `jlinux-visual`. O diretório é descoberto por versões compatíveis do Codex. Essas skills preservam invariantes específicos do projeto e não substituem bibliotecas, ferramentas ou testes.

## Skills baixadas do GitHub

Instaladas no ambiente do usuário em `~/.codex/skills`, com atalhos de descoberta em `~/.agents/skills` quando não havia uma skill com o mesmo nome. Não são dependências do binário JLinux.

| Skill | Origem | Uso |
| --- | --- | --- |
| gh-fix-ci | openai/skills | Diagnóstico e correção de checks GitHub Actions |
| gh-address-comments | openai/skills | Tratar comentários de revisão quando solicitado |
| address-sanitizer | trailofbits/skills | Instrumentar e investigar erros de memória |
| libfuzzer | trailofbits/skills | Fuzzing de parsers e APIs C isoláveis |
| differential-review | trailofbits/skills | Revisão de segurança de mudanças relevantes |
| property-based-testing | trailofbits/skills | Invariantes de geometria, configuração e animações; escolher ferramentas compatíveis com C |

As duas skills OpenAI foram instaladas de `main` em 05/11/2026; o commit observado após instalação foi `49f948faa9258a0c61caceaf225e179651397431` (não equivale a pin da instalação original). As quatro Trail of Bits foram instaladas com ref fixada `470419e16f97e112d01fc32f4898aea1755015d9`. Hashes dos arquivos instalados estão em `docs/skills-lock.json`.

Fontes: https://github.com/openai/skills e https://github.com/trailofbits/skills. Conteúdo externo foi inspecionado antes da instalação. Não foram executados scripts baixados das skills. Instalar uma skill não instala Clang/libFuzzer ou executa uma auditoria. Exemplos devem ser verificados contra as ferramentas reais.

A skill `c-review` foi descartada nesta seleção porque depende de workflow e agentes de outro ambiente. Skills de cloud, web e smart contracts não são necessárias para o compositor. Não há vantagem em instalar um catálogo inteiro.

Para reproduzir instalações, usar o Skill Installer com repo/path/ref do manifesto. Após instalar, as skills estarão disponíveis no próximo turno em clientes compatíveis; se não aparecerem, recarregar o cliente. Não sobrescrever skills existentes automaticamente.

## Plugins iniciais

- **GitHub:** integração de repositório, PRs e CI. Disponível para instalar/conectar; ainda não confirmado. O CLI `gh` já está autenticado e é o caminho operacional atual.
- **Context7:** documentação auxiliar. Disponível para instalar/conectar; ainda não confirmado. Cobertura de wlroots precisa ser verificada; headers locais e documentação oficial são a referência.

Não há plugin obrigatório para compilar. As conexões dependem da conta e não ficam ativas só por serem documentadas. Não foram alteradas permissões de apps.

## Referências de configuração

- https://learn.chatgpt.com/docs/agent-configuration/subagents
- https://learn.chatgpt.com/docs/build-skills
