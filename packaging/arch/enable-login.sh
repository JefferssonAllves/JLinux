#!/bin/sh
set -eu
if [ "$(id -u)" != 0 ]; then
    echo 'Execute este configurador com sudo.' >&2
    exit 1
fi
for package in jlinux-desktop-git greetd cage; do
    pacman -Q "$package" >/dev/null
done
for binary in /usr/bin/jlinux-greeter /usr/bin/jlinux-session /usr/bin/cage; do
    test -x "$binary"
done
manager=$(systemctl show display-manager.service -p Id --value 2>/dev/null || true)
if [ -n "$manager" ] && [ "$manager" != greetd.service ] && systemctl cat display-manager.service >/dev/null 2>&1; then
    echo 'Outro gerenciador de login está configurado; preserve-o e faça a migração manual.' >&2
    exit 1
fi
umask 077
backup=$(mktemp -d /var/lib/jlinux-login-backup.XXXXXXXX)
if [ -e /etc/greetd/config.toml ]; then cp -a /etc/greetd/config.toml "$backup/config.toml"; fi
systemctl get-default > "$backup/default-target"
systemctl is-enabled greetd.service > "$backup/greetd-enabled" 2>/dev/null || true
systemctl is-enabled getty@tty2.service > "$backup/tty2-enabled" 2>/dev/null || true
temporary=$(mktemp /etc/greetd/.jlinux.XXXXXXXX)
trap 'rm -f "$temporary"' EXIT HUP INT TERM
install -m 0644 /usr/share/jlinux/greetd.toml "$temporary"
mv -f "$temporary" /etc/greetd/config.toml
systemctl enable --now getty@tty2.service
systemctl enable greetd.service
systemctl set-default graphical.target
printf 'Login JLinux habilitado para a próxima inicialização. Backup: %s\n' "$backup"
echo 'Recuperação: Ctrl+Alt+F2. Nenhuma sessão atual foi encerrada.'
