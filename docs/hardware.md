# Hardware observado

Coleta em 04/11/2026 neste ambiente Linux. Valores expostos pelo kernel, sem benchmark.

| Componente | Resultado |
| --- | --- |
| CPU | AMD Ryzen 7 5700U, x86_64, 8 núcleos / 16 threads |
| RAM utilizável | 7,1 GiB; cerca de 5,2 GiB disponíveis na coleta |
| Swap | 11 GiB reportados por free; partição de 12 GiB |
| GPU | Radeon integrada Lucienne, driver amdgpu ativo |
| Disco | SSD NVMe SSSTC CL1-4D256, 238,5 GiB |
| Sistema | Arch Linux rolling |
| Tela | Modos incluem 1600×900 e 1920×1080; modo ativo/frequência não verificados |
| Wi-Fi | Realtek RTL8822CE, driver rtw88_8822ce |

A RAM é o principal recurso a preservar. A GPU integrada compartilha recursos de memória. A quantidade de RAM instalada e a causa da diferença para a utilizável não foram confirmadas. Não inferimos frequência real de CPU dos limites reportados pelo driver.

Não incluímos números de série, MACs, UUIDs ou credenciais. scripts/collect-hardware.sh permite repetir a coleta. O ambiente inicial não possui compilador, Meson, GTK ou Sway.
