# Filtro de anúncios do navegador JLinux

Fonte dos domínios: [AdAway default blocklist](https://github.com/AdAway/adaway.github.io), snapshot do commit `f39ba0a523564447dfa94e40327c1a14c02304dc`, obtido em 06/11/2026. Autores: Kicelo, Dominik Schuermann e os demais contribuidores registrados no histórico do projeto. Licença dos dados: Creative Commons Attribution 3.0; texto em `COPYING.AdAway`. O arquivo `adaway-hosts.txt` preserva os créditos originais.

Modificações JLinux: converte os hosts em regras WebKit para recursos de terceiros, elimina subdomínios já cobertos, adiciona bloqueio do caminho `/pagead/` para scripts/imagens/fetch e ocultação de três marcadores específicos de anúncios Google. `blocker.json` é uma adaptação dos dados, sob CC BY 3.0; não é a lista completa do uBlock/EasyList. Não contém exceções pagas.

Regenerar offline, a partir da raiz do repositório:

```sh
python3 scripts/generate-browser-blocklist.py data/browser/adaway-hosts.txt data/browser/blocker.json
```

Para atualizar, revisar um novo snapshot da fonte, registrar o commit e os créditos, regenerar e executar os testes do navegador. A atualização é distribuída com o pacote; o navegador não consulta serviços externos para obter filtros nem envia histórico ao mantenedor. A lista é embutida como recurso comprimido e compilada pelo próprio WebKit em cache local.

Limites: cobre os domínios selecionados e padrões específicos acima. Anúncios no mesmo domínio do conteúdo, anúncios de vídeo como os do YouTube, novos servidores e técnicas de antibloqueio podem continuar aparecendo. Não implementa scripts de contorno ou proteção universal contra rastreamento.
