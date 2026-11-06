# Consolidação dos pedidos do usuário

Esta integração reúne os pedidos de produto ainda válidos sobre a instalação existente. Branch ou PR aberto não significa que o código já está no pacote. Uma escolha posterior do usuário prevalece sobre versões anteriores.

| Pedido | Fonte | Resultado nesta integração |
| --- | --- | --- |
| Tom rosado discreto nas bordas | #54 / b55aaa2 | Cor mais recente: stop RGB (.84, .24, .91), combinado com o contorno definido |
| Cabeçalho quase preto e contorno nítido | #49 / bc702d5 | Base translúcida a 88%, traço 1,6 px e halo 2,2 px a 18% |
| Barra superior reta, sem margem e com fade inferior | #47 / f59e445 | 36 px de controles + 16 px de fade; sem linha inferior e sem menus à esquerda |
| Remover completamente o dock | #57 / c81b589 | Mantido: sem superfície, seletor, configuração ou reserva lateral |
| Encaixe lateral manual em vez de organização automática | #44 / 6df67d0 | Mantido; nenhuma restauração do tiling automático de #38/#42 |
| Volume, energia, Arquivos, pastas pessoais e idioma | Base instalada | Mantidos, sem reintroduzir lançadores removidos |
| Arquivos como padrão para pastas | #58 / 850294f | Mantido, incluindo xdg-utils e precedência das preferências pessoais |
| Navegador: abas, vídeo, pesquisa, adblock, cabeçalho, downloads e imagens Blob | #32/#36/#40/#46/#48/#50/#52/#53/#59/#60 | Já contidos no código instalado 21be53d; branch posterior difere somente em documentação |

Não foram reativados docks antigos, rótulos de menus ou layouts substituídos. V1/V2 rejeitados, propostas de arquitetura e itens futuros de TASKS.md não são funcionalidades pendentes de instalação. As fotos originais continuam como referência visual; não há promessa de reprodução integral.

Os helpers de pintura de #47/#49 e a cor de #54 são reutilizados sobre a base sem dock. O shell mantém os botões transitórios de volume/energia nas posições anteriores. Só a reserva superior muda de 50 para 52 px, com testes de maximização e snap atualizados. BLUR/TINT/REFLECTION/SHADOW continuam independentes; o vidro do shell usa apenas o wallpaper em cache.

Validação de código: 20 cenários release e os mesmos 20 com ASan/UBSan, incluindo teste específico do tom rosado, cabeçalho preto/translúcido, fade, área útil, ausência do dock, encaixe lateral e controles preservados. A prévia nativa atual foi inspecionada em 1672×941; a moldura foi comparada antes/depois. Não são capturas da sessão. Navegador não foi modificado ou retestado nesta integração; o pacote instalado foi auditado por commit de origem.

Instalação e ativação são etapas separadas: o pacote novo não substitui o compositor/shell já em execução. Sem recarga segura, é necessário novo login. Não encerrar aplicativos sem combinar com o usuário. A versão efetivamente instalada e os checks do pacote são registrados após a instalação em TASKS.md e docs/arch-installation.md.
