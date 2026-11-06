# Aplicativos no dock

O botão **+** na parte inferior do dock abre uma lista nativa dos aplicativos instalados. Clique no nome para fixar; clique novamente para remover. O marcador ✓ indica os aplicativos adicionais fixados. Fechar volta ao desktop. Role a lista para ver os demais aplicativos e role sobre o dock para acessar os atalhos que não cabem nas três posições visíveis. Ao fixar um aplicativo, o dock passa a mostrar o final da lista.

Arquivos, Terminal e Codex continuam sendo as três ações iniciais. Os atalhos adicionais usam a inicial do nome em um bloco azul/violeta; ainda não há carregador de temas de ícones. A lista mostra nomes completos. Arrastar/reordenar atalhos, busca e navegação por teclado na lista ainda não estão implementados.

São listadas entradas .desktop visíveis ao ambiente, dos diretórios XDG de aplicativos (incluindo ~/.local/share/applications). Isso não instala programas: um executável sem entrada .desktop precisa dessa entrada para aparecer. A abertura segue Exec, argumentos e campos da entrada, usando GDesktopAppInfo, sem montar um comando de shell. Entradas Terminal=true dependem de um terminal reconhecido pelo GIO. Não há inicialização automática dos aplicativos fixados.

Preferências por usuário ficam em ~/.config/jlinux/dock.ini (ou sob XDG_CONFIG_HOME):

```ini
[Dock]
Apps=org.exemplo.App.desktop;
```

O arquivo é substituído atomicamente, com criação em modo 0600; falha de gravação preserva o estado anterior. São aceitos até 32 aplicativos adicionais. IDs duplicados são eliminados ao carregar; configuração inválida é preservada, com edição desativada e as três ações básicas disponíveis. Entradas de aplicativos removidos permanecem salvas e aparecem com ícone genérico; remova o ID do arquivo quando o aplicativo já não estiver na lista.

O shell carrega as preferências ao iniciar e ao abrir a lista. Não há observação contínua do arquivo ou sincronização entre dois docks simultâneos. Instalações/alterações de entradas .desktop podem exigir novo processo do shell para atualizar o catálogo mantido pela GLib.

## Ativação sem encerrar a sessão

`jlinux-shell --dock-panel` executa somente o novo dock como sobreposição, sem reservar área extra ou iniciar compositor/wallpaper/barra superior. É um modo temporário para uma sessão que ainda executa um shell antigo: cobre a superfície do dock anterior com uma base opaca, mantendo as janelas e os processos existentes. Usa o primeiro output e sua escala inteira; multimonitor, rotação e escala fracionária ainda não foram validados nesse modo. Ao encerrar esse processo auxiliar, o dock anterior reaparece. No próximo login, o shell novo usa o dock integrado e dispensa o auxiliar.

O modo integrado mantém o material com fundo do wallpaper do shell. A sobreposição temporária tem fundo próprio, sem amostragem de janelas e sem alegar blur sobre o desktop real. Não iniciar dois auxiliares ao mesmo tempo.

## Validação

Testes headless usam HOME/XDG isolados, aplicativo fictício e assert de máscara de sinais no filho. Cobrem fixar, abrir, persistir após reiniciar, acessar pelo scroll, remover e preservar configuração inválida. O mesmo fluxo roda no shell completo e no modo --dock-panel. A prévia em design/native-dock-apps-preview.png é renderizada pelas rotinas do shell, não uma captura da sessão.
