#!/bin/sh
# Preserve rollback packages and let the system age temporary files safely.
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo 'Execute como root.' >&2
    exit 1
fi
if [ -e /var/lib/pacman/db.lck ]; then
    echo 'Pacman ocupado; limpeza adiada.'
    exit 0
fi
/usr/bin/paccache --remove --keep 2
/usr/bin/systemd-tmpfiles --clean
orphans=$(/usr/bin/pacman -Qdtq || :)
if [ -n "$orphans" ]; then
    printf 'Dependências órfãs para revisão manual:\n%s\n' "$orphans"
fi
