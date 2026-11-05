# Instruções de trabalho no JLinux

- Ler CONTRIBUTING.md e seguir os destinos de PR definidos nele.
- Não editar diretamente em main ou develop. Criar uma branch temporária apropriada antes de alterar arquivos.
- Não fazer push direto em main/develop, merge de PR, publicar release ou criar tag de versão sem autorização explícita do usuário para essa ação.
- O pedido de implementar trabalho autoriza criar a branch, commits e PR para revisão; não implica integrar o PR.
- Conferir status, branches remotas e alterações locais antes de começar. Preservar trabalho existente.
- Usar commits coesos; atualizar a documentação quando comportamento ou arquitetura mudar.
- Rodar checks relevantes. Relatar claramente o que foi testado e o que continua pendente.
- Não afirmar que personalização, animações, efeitos ou adaptação estão disponíveis enquanto não estiverem implementados e validados.
- O produto terá compositor Wayland próprio, personalização ampla e adaptação ao hardware respeitando as preferências do usuário.
