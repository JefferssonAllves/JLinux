"""Fixed dock shortcuts and navigation keys; no shell commands supplied by HTTP."""
import asyncio
import configparser
import os
from pathlib import Path
import socket
import stat

from aiohttp import web

# shortcut: indices follow the installed native dock API; update APPS when that protocol changes.
APPS = {
    0: ('Brave', 'brave-browser.desktop', 'brave-desktop'),
    1: ('Terminal', 'foot.desktop', 'foot'),
    2: ('Neovim', 'Neovim-Code.desktop', 'nvim'),
    3: ('Arquivos', 'org.jlinux.Files.desktop', 'system-file-manager'),
    5: ('Roblox', 'org.vinegarhq.Sober.desktop', 'org.vinegarhq.Sober'),
    6: ('Monitor', 'btop.desktop', 'btop'),
    7: ('Spotube', 'com.github.KRTirtho.Spotube.desktop', 'org.jlinux.Spotube'),
    8: ('WhatsApp', 'org.jlinux.WhatsApp.desktop', 'org.jlinux.WhatsAppDock'),
    9: ('Netflix', 'org.jlinux.Netflix.desktop', 'org.jlinux.Netflix'),
}
KEYS = {name: ['-k', symbol] for name, symbol in {
    'up': 'Up', 'down': 'Down', 'left': 'Left', 'right': 'Right', 'ok': 'Return', 'back': 'Escape', 'tab': 'Tab'}.items()}
KEYS['switch'] = ['-M', 'alt', '-k', 'Tab', '-m', 'alt']


class RemoteControls:
    def __init__(self, desktop, home=None, data_dirs=None):
        self.desktop = desktop
        home = Path(home) if home else Path.home()
        self.data_dirs = data_dirs if data_dirs is not None else [home/'.local/share', home/'.local/share/flatpak/exports/share',
            Path('/usr/local/share'), Path('/usr/share'), Path('/var/lib/flatpak/exports/share')]
        self.icons = {}

    def index(self, value):
        if not isinstance(value, str) or value not in {str(index) for index in APPS}:
            raise web.HTTPBadRequest(text='Aplicativo inválido')
        return int(value)

    def apps(self):
        return [{'id': str(index), 'name': name, 'icon': f'/api/remote/icon/{index}'} for index, (name, _, _) in APPS.items()]

    def icon(self, index):
        if index in self.icons:
            return self.icons[index]
        name, desktop, fallback = APPS[index]
        icon = fallback
        for root in self.data_dirs:
            path = root/'applications'/desktop
            if not path.is_file():
                continue
            config = configparser.ConfigParser(interpolation=None, strict=False)
            try:
                config.read(path)
                icon = config.get('Desktop Entry', 'Icon', fallback=fallback)
            except configparser.Error:
                pass
            break
        if index in (7, 8):
            icon = fallback
        paths = [Path(icon)] if os.path.isabs(icon) else []
        for candidate in dict.fromkeys([icon, fallback]):
            if '/' in candidate:
                continue
            for root in self.data_dirs:
                for theme in ['hicolor', 'Adwaita', 'AdwaitaLegacy']:
                    for size in ['128x128', 'scalable', '96x96', '64x64', '48x48', '32x32', '256x256']:
                        for context in ['apps', 'places', 'categories', 'devices', 'actions', 'legacy']:
                            paths.extend(root/'icons'/theme/size/context/(candidate+ext) for ext in ['.png', '.svg'])
                paths.extend(root/'pixmaps'/(candidate+ext) for ext in ['.png', '.svg'])
        for path in paths:
            try:
                if path.suffix not in ('.svg', '.png') or not path.is_file() or path.stat().st_size > 512*1024:
                    continue
                result = (path.read_bytes(), 'image/svg+xml' if path.suffix == '.svg' else 'image/png')
                self.icons[index] = result
                return result
            except OSError:
                continue
        result = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="16" fill="#49344f"/><text x="32" y="43" text-anchor="middle" font-size="34" fill="#eee9ff">{name[0]}</text></svg>'.encode(), 'image/svg+xml')
        self.icons[index] = result
        return result

    def launch(self, value):
        index = self.index(value)
        env = self.desktop()
        try:
            runtime = Path(env['XDG_RUNTIME_DIR'])
            info = runtime.stat()
            path = runtime/'jlinux-window-control'
            target = path.stat()
            if not runtime.is_absolute() or info.st_uid != os.getuid() or info.st_mode & 0o077 or not stat.S_ISDIR(info.st_mode) or target.st_uid != os.getuid() or not stat.S_ISSOCK(target.st_mode):
                raise OSError()
            with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as control:
                control.setblocking(False)
                control.sendto(bytes([index]), str(path))
        except (KeyError, OSError):
            raise web.HTTPServiceUnavailable(text='Dock indisponível. Entre na sessão do JLinux.')
        return {'ok': True}

    async def key(self, action):
        if not isinstance(action, str) or action not in KEYS:
            raise web.HTTPBadRequest(text='Tecla inválida')
        env = self.desktop()
        if not env:
            raise web.HTTPServiceUnavailable(text='Entre na sessão gráfica primeiro')
        try:
            process = await asyncio.create_subprocess_exec('wtype', *KEYS[action], env=dict(os.environ, **env),
                stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        except OSError:
            raise web.HTTPServiceUnavailable(text='Navegação indisponível')
        try:
            if await asyncio.wait_for(process.wait(), 3):
                raise web.HTTPServiceUnavailable(text='Não foi possível enviar a tecla')
        except (TimeoutError, asyncio.CancelledError) as error:
            if process.returncode is None:
                process.kill()
            await process.wait()
            if isinstance(error, TimeoutError):
                raise web.HTTPServiceUnavailable(text='O aplicativo não respondeu à navegação')
            raise
        return {'ok': True}
