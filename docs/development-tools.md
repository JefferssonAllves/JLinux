# Ferramentas locais de desenvolvimento

Instalação realizada em 05/11/2026. Essas ferramentas pertencem ao ambiente de desenvolvimento; não são dependências nem serviços do desktop JLinux.

| Ferramenta | Versão verificada | Instalação | Estado |
| --- | --- | --- | --- |
| Headroom | 0.42.0 | `uv tool install --python 3.13 'headroom-ai[mcp]'` | CLI instalada; proxy e compressão automática não ativados |
| Graphify | 0.9.102 | `uv tool install --python 3.13 graphifyy` | CLI e skill Codex instaladas; nenhum grafo do projeto gerado |
| CodeBurn | 0.9.27 | `npm install --global --prefix "$HOME/.local" codeburn` | CLI instalada; dashboard/compartilhamento não iniciados |
| Ponytail | 5.3.3 | Marketplace oficial e `codex plugin add ponytail@ponytail` | Plugin instalado e habilitado; carregar em nova sessão, com confiança de hooks gerenciada pelo cliente |

Foram instalados `uv`, `nodejs` e `npm` pelo pacman. As ferramentas Python têm ambientes isolados com Python 3.13.18, sem modificar o Python do sistema. Executáveis do usuário ficam em `~/.local/bin`.

## Uso

```sh
headroom --help
graphify --help
codeburn --help
codex plugin list --json
```

Graphify: a skill foi instalada com `graphify install --platform codex` em `~/.codex/skills/graphify`, com link de descoberta em `~/.agents/skills/graphify`. No próximo turno ou após recarregar um cliente compatível, pode ser invocada como `$graphify`. Não foram inseridos hooks ou instruções Graphify no AGENTS.md do JLinux.

Ponytail: marketplace adicionado com `codex plugin marketplace add DietrichGebert/ponytail`. O cliente pode exigir confiança nos hooks pelo menu `/hooks`; instalar/habilitar o plugin não comprova que seus hooks rodaram nesta sessão. As regras do JLinux e as decisões explícitas do usuário continuam prioritárias, incluindo compositor e toolkit próprios.

Headroom: instalados recursos CLI/MCP, sem extras de ML, serviço persistente ou redirecionamento do tráfego do Codex. Os comandos de verificação usaram `HEADROOM_OFFLINE=1`. Disponibilidade do executável não comprova integração de compressão nem economia de tokens. Configurar proxy/MCP é uma etapa distinta; não foi feita nesta instalação.

CodeBurn: lê dados locais de sessões quando executado para análise. A instalação não iniciou análise, compartilhamento, painel web ou pareamento de dispositivos.

## Verificações e limites

- `headroom --version` e `headroom --help`: passaram (modo offline).
- `graphify --version` e ajuda de instalação: passaram; instalador confirmou skill e referências.
- `codeburn --version` e `codeburn --help`: passaram.
- `codex plugin list --json`: confirmou Ponytail 5.3.3 instalado e habilitado.
- Não foram executados benchmarks ou verificadas alegações de economia dos projetos.

## Fontes oficiais

- Headroom: https://github.com/headroomlabs-ai/headroom
- Graphify: https://github.com/Graphify-Labs/graphify (pacote Python `graphifyy`, com dois `y`)
- CodeBurn: https://github.com/getagentseal/codeburn (o endereço antigo Jlsiles/codeburn redireciona para este projeto)
- Ponytail: https://github.com/DietrichGebert/ponytail

Os comandos acima reproduzem os métodos de instalação, mas sem fixação de versão baixarão as versões disponíveis naquele momento. Para reinstalar exatamente as CLIs registradas, usar `headroom-ai[mcp]==0.42.0`, `graphifyy==0.9.102` e `codeburn@0.9.27`. As dependências transitivas não foram congeladas neste documento.

## Ferramentas de design adicionadas

- **shadcn 4.23.0:** CLI instalada em ~/.local e MCP registrado no Codex com `shadcn mcp`. Não foi inicializado um projeto React no JLinux.
- **Magic MCP 0.2.3 / plugin 21st 1.0.1:** pacote de compatibilidade instalado e plugin oficial habilitado. O Magic foi sucedido por 21st MCP. Requer uma chave `API_KEY_21ST`, ausente no ambiente verificado; autenticação e acesso aos componentes continuam pendentes. Não foram gravadas chaves em arquivos do projeto.
- **frontend-design:** skill oficial de anthropics/skills, ref `ca45fc496a3436251f191354091d557da31c15b5`.
- **find-skills:** descoberta e instalação sob demanda, de vercel-labs/skills, ref `e80e8255bb89ce3288edc34d6cead20cbb7cc521`. Complementa o Skill Installer já disponível; não é um processo permanente.
- **liquid-glass-design:** referência ao efeito de vidro do iPhone, de affaan-m/ECC, ref `b38be6a7f6350964d59328b52f0624e1d1d552d1`. A skill descreve APIs SwiftUI/UIKit/WidgetKit: serve como referência de Apple Liquid Glass, não implementa esse efeito no compositor Linux.

As skills foram instaladas em ~/.codex/skills, com links de descoberta em ~/.agents/skills. Carregar no próximo turno ou recarregar o cliente. shadcn e 21st atendem componentes web; o desktop permanece nativo em C. A instalação dessas ferramentas não entrega temas, componentes de vidro ou efeitos no JLinux.

Fontes: https://ui.shadcn.com/docs/mcp, https://github.com/21st-dev/magic-mcp, https://github.com/anthropics/skills, https://github.com/vercel-labs/skills, https://github.com/affaan-m/ECC.

Verificado: versão e ajuda de shadcn, CLI registrada em `codex mcp list`, pacote npm Magic e plugin 21st instalados, três diretórios de skills presentes. O handshake MCP será verificado separadamente; 21st depende de autenticação.
