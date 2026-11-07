# Visão de janelas — Super+Tab

Super+Tab abre uma visão das janelas do espaço de trabalho atual. Ela permanece aberta depois de soltar Super para permitir a seleção por mouse. Clique em uma miniatura ou use Enter para focar a janela escolhida. Esc ou clique no espaço vazio cancela e restaura o foco anterior.

- Tab/Super+Tab e Shift+Tab percorrem as janelas; setas navegam na grade.
- Page Up/Page Down ou roda do mouse mudam de página.
- Até nove miniaturas por página, centralizadas e mantendo proporção. Uma janela isolada tem miniatura limitada a 560×330, em vez de preencher a tela.
- As miniaturas mostram os buffers existentes no momento em que a página é construída. Não são vídeos atualizados continuamente.
- Abrir/fechar uma janela encerra a visão para reconstruir a lista com segurança no próximo acionamento. A visão vazia pode ser fechada normalmente.

O visual usa os materiais nativos de janela: tonalização escura, contorno azul/rosado, títulos e destaque da seleção. O shell permanece em processo separado; essa interface pertence à gestão de janelas do compositor, como as molduras. O dock radial fica oculto durante a seleção. Teclado/cliques são consumidos pela visão e não enviados aos aplicativos ao fundo. Ctrl+Alt+Fn mantém seu encaminhamento existente; não foi testado fisicamente nesta entrega.

## Desfoque e recursos

Ao abrir, o renderer compõe uma imagem da saída central sem os overlays do shell, usando wlroots. A GPU reduz essa imagem para 1/4 de cada dimensão antes de ler os pixels. O box blur separável existente roda nessa imagem reduzida, que é mantida em cache até fechar. Não há captura ou blur por miniatura, por hover ou por troca de página. O fundo opaco permite à cena descartar a renderização do desktop encoberto. A operação exige composição durante a visão; após fechar, a política normal de apresentação é restaurada.

As miniaturas mantêm referências aos buffers, preservam a proporção e não recebem entrada diretamente. Somente nove conjuntos de referências são retidos por vez; trocar página e fechar liberam os anteriores com o ciclo de vida wlroots. O compositor não grava imagens das janelas no disco. Não há timer/polling de atualização nem animação nova. As transições de janela pendentes são finalizadas ao entrar.

JLINUX_GLASS_BLUR=0 desativa captura e blur e mantém um véu escuro transparente sobre o desktop. TINT/SHADOW/REFLECTION continuam controlando o material dos cartões separadamente. Se a captura falhar, a grade continua funcional com fundo escuro e um aviso visível de desfoque indisponível. Isso não é apresentado como blur bem-sucedido. Em resolução insuficiente, miniaturas podem não caber; a resolução validada é 1280×720 e a política atual segue a saída central. HDR, escala fracionária e multimonitor não foram validados.

## Validação e custo observado

A bateria local inclui renderer/ownership, teclado, clique, páginas, roda, ausência de recaptura ao paginar, isolamento das teclas, desaparecimento de uma janela, desconexão do cliente e visão vazia. A fixture usa dez janelas e um buffer compartilhado; não representa dez aplicativos completos. ASan/UBSan verifica liberação dos recursos. A prévia design/window-overview.png usa janelas ilustrativas e blur desativado para testar o desenho; não é captura da sessão real.

Teste adicional em headless/GLES2 no AMD Radeon Graphics (radeonsi, renoir, ACO), Mesa e wlroots 0.20.2 confirmou captura/desfoque e seleção sem trocar VT ou controlar uma saída DRM física. Ambiente: Ryzen 7 5700U, kernel 7.2.14-arch1-1, 1280×720, alimentação externa online; frequência/carga de outros processos não foram controladas.

Observação de preparação (captura/cache + construção da grade), quatro aberturas com dez janelas por perfil, executadas sequencialmente em release:

| Perfil | Mediana | Mínimo–máximo |
| --- | --- | --- |
| Blur ativo, headless/GLES2 | 29,72 ms | 28,66–38,30 ms |
| Blur desativado, headless/GLES2 | 25,15 ms | 23,71–29,32 ms |

São amostras descritivas do teste, não latência tecla→apresentação, percentis de quadro ou comparação estatística. Não medem CPU em repouso, PSS total, energia ou fluidez na saída física. Próxima medição: repetir o mesmo cenário com aplicativos reais em sessão aninhada, coletando tempo de quadro/PSS e separando custo de captura, blur e cartões. Sanitizadores não entram na comparação de velocidade.

Reproduzir o fluxo com GPU em headless, após compilar:

```sh
WLR_RENDERER=gles2 WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128 scripts/smoke-overview.py build/jlinux-compositor build/jlinux-test-layout-client /usr/bin/wtype build/jlinux-test-pointer build/jlinux-shell
```

O caminho do render node depende da máquina. O teste normal usa Pixman; a variante overview-no-blur confirma que nenhuma captura é realizada com o efeito desativado. Instalar o pacote não troca o compositor ativo: a função entra no próximo login.
