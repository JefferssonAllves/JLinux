---
name: jlinux-compositor
description: Implementar ou depurar o compositor Wayland do JLinux em C/wlroots 0.20, incluindo saídas, protocolos, listeners e ciclo de vida. Não usar para alterações apenas de documentação.
---

# jlinux-compositor

Leia src/compositor/, meson.build e os headers instalados antes de escolher APIs. Confirme a versão via pkg-config --modversion wlroots-0.20.

Para cada objeto novo, identifique dono, evento de destruição, listeners e sequência de limpeza em falha parcial. Cliente desconectado e saída removida não podem deixar ponteiros vivos. Trabalhe com capacidades reais do backend; não force configurações de GPU pelo nome do hardware.

Preserve separação entre compositor e shell. Programe quadros quando a cena mudar ou uma animação estiver ativa; evite polling global. DMA-BUF exige formatos e sincronização corretos; direct scanout exige elegibilidade e fallback.

Compile e execute meson test no build relevante. Para mudanças de ownership, use build separado com -Db_sanitize=address,undefined quando suportado. Não transforme sucesso headless em afirmação de compatibilidade DRM/GPU. Atualize TASKS.md apenas com evidência.
