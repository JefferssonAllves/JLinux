# Fluxo de desenvolvimento

## Branches

| Branch | Finalidade | Origem | Destino do PR |
| --- | --- | --- | --- |
| `main` | Histórico estável e versões publicadas | — | — |
| `develop` | Integração do próximo ciclo | `main` na criação inicial | `main` por release |
| `feature/<descricao>` | Funcionalidade isolada | `develop` | `develop` |
| `fix/<descricao>` | Correção do ciclo atual | `develop` | `develop` |
| `chore/<descricao>` | Infraestrutura, manutenção ou documentação | `develop` | `develop` |
| `release/<versao>` | Estabilização de uma versão | `develop` | `main` |
| `hotfix/<descricao>` | Correção urgente de versão publicada | `main` | `main`, depois sincronizar `develop` |

Usar nomes curtos em minúsculas, com hífens. Não fazer push direto em `main` ou `develop` após a criação inicial dessas branches. Não versionar binários, credenciais ou arquivos locais.

## Ciclo de trabalho

1. Atualizar a referência remota e criar a branch a partir da origem correta.
2. Fazer commits coesos que expliquem a intenção. Usar prefixos `feat:`, `fix:`, `docs:`, `refactor:`, `test:` ou `chore:`.
3. Compilar e executar os testes relevantes. Separar resultados reais de metas, hipóteses e testes pendentes.
4. Abrir PR para o destino correto. Usar draft enquanto houver trabalho necessário, falhas ou verificações pendentes. Descrever comportamento, motivação, validação e limitações relevantes.
5. Integrar somente após CI verde e revisão do responsável pelo projeto. Mudanças pequenas de funcionalidade/manutenção podem usar squash; releases e sincronizações entre branches duradouras devem preservar ancestralidade com merge commit.
6. Apagar a branch temporária após integração, quando não for mais necessária. Não reescrever branches compartilhadas nem usar force push sem autorização explícita.

## Releases e hotfixes

Criar `release/<versao>` de `develop`; aceitar apenas correções de estabilização nessa branch. Abrir PR para `main`. Após integração e validação, criar tag anotada `v<versao>` e release com notas e limitações. Não publicar versões só porque o build passou. Sincronizar `main` de volta em `develop` por PR com merge commit para incluir correções da release.

Para hotfix, partir de `main`, validar e abrir PR para `main`. Após integrar, abrir PR de sincronização de `main` para `develop` e, se houver, para a release em andamento.

## Validação local

```sh
meson setup build --buildtype=release
meson compile -C build
meson test -C build --print-errorlogs
git diff --check
```

Se `build` já existir, reconfigurar com `meson setup build --reconfigure`. Ativar `-Dpanel=true` quando alterar o painel. O teste headless verifica o ciclo de vida do compositor; não comprova correção visual, entrada ou desempenho em GPU.

## Regras e proteção no GitHub

Este documento estabelece o fluxo da equipe. Proteção técnica de branches deve exigir PR e o check `build` em `main` e `develop`, bloquear exclusão e force push e, quando houver revisores disponíveis, exigir aprovação. A existência deste arquivo não ativa essas opções no GitHub. As regras efetivamente configuradas devem ser verificadas separadamente.
