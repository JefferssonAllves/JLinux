#!/bin/sh
set -eu
for tool in lscpu free lsblk lspci; do
    command -v "$tool" >/dev/null || { echo "Ferramenta ausente: $tool" >&2; exit 1; }
done
lscpu
free -h
lsblk -o NAME,TYPE,SIZE,ROTA,MODEL
lspci -nnk
cat /etc/os-release
for modes in /sys/class/drm/*/modes; do
    [ -r "$modes" ] || continue
    printf '\n%s\n' "$modes"
    cat "$modes"
done
