#!/bin/sh
set -eu
[ "$(id -u)" != 0 ] || { echo 'Execute sem sudo.' >&2; exit 1; }
connect_source=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for connect_command in python3 ttyd wayvnc tailscale wpctl; do
    command -v "$connect_command" >/dev/null || { echo "Falta instalar: $connect_command" >&2; exit 1; }
done
python3 -c 'import aiohttp'
python3 "$connect_source/fetch-vendor.py"
connect_target="$HOME/.local/share/jlinux-connect"
mkdir -p "$connect_target" "$HOME/.local/bin" "$HOME/.config/systemd/user" "$HOME/.config/jlinux"
cp "$connect_source/server.py" "$connect_source/cli.py" "$connect_source/system_controls.py" "$connect_source/file_transfer.py" "$connect_source/README.md" "$connect_target/"
cp -R "$connect_source/web" "$connect_target/"
cp "$connect_source/jlinux-connect.service" "$HOME/.config/systemd/user/"
cat > "$HOME/.local/bin/jlinux-connect" <<'EOF'
#!/bin/sh
exec /usr/bin/python3 "$HOME/.local/share/jlinux-connect/cli.py" "$@"
EOF
chmod 755 "$HOME/.local/bin/jlinux-connect"
umask 077
: > "$HOME/.config/jlinux/connect-enabled"
sudo /usr/bin/loginctl enable-linger "$(id -un)"
connect_runtime="/run/user/$(id -u)"
XDG_RUNTIME_DIR="$connect_runtime" DBUS_SESSION_BUS_ADDRESS="unix:path=$connect_runtime/bus" systemctl --user daemon-reload
XDG_RUNTIME_DIR="$connect_runtime" DBUS_SESSION_BUS_ADDRESS="unix:path=$connect_runtime/bus" systemctl --user enable jlinux-connect.service
XDG_RUNTIME_DIR="$connect_runtime" DBUS_SESSION_BUS_ADDRESS="unix:path=$connect_runtime/bus" systemctl --user restart jlinux-connect.service
echo 'Instalado. Execute jlinux-connect conectar para vincular o iPhone pelo Tailscale.'
