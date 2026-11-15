"""Control the same MPRIS players used by JLinux's native top bar."""
import asyncio
import contextlib
import hashlib
import json
import os
import re

from aiohttp import web

PREFIX = 'org.mpris.MediaPlayer2.'
INTERFACE = 'org.mpris.MediaPlayer2.Player'
PATH = '/org/mpris/MediaPlayer2'
METHODS = {'play_pause': 'PlayPause', 'previous': 'Previous', 'next': 'Next', 'seek_back': 'Seek', 'seek_forward': 'Seek'}


class MediaControls:
    def __init__(self, desktop):
        self.desktop = desktop
        self.players = {}
        self.lock = asyncio.Lock()

    async def call(self, address, name, interface, method, *arguments):
        process = await asyncio.create_subprocess_exec('/usr/bin/busctl', '--address=' + address,
            '--timeout=2', '--json=short', 'call', name, PATH if name != 'org.freedesktop.DBus' else '/org/freedesktop/DBus',
            interface, method, *arguments, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        try:
            out, _ = await asyncio.wait_for(process.communicate(), 3)
        except (TimeoutError, asyncio.CancelledError):
            with contextlib.suppress(ProcessLookupError):
                process.kill()
            await process.wait()
            raise
        if process.returncode:
            raise OSError('Player indisponível')
        return json.loads(out) if out else {}

    async def properties(self, address, name):
        result = await self.call(address, name, 'org.freedesktop.DBus.Properties', 'GetAll', 's', INTERFACE)
        data = result.get('data', [{}])[0]
        return {key: value.get('data') for key, value in data.items()}

    async def status(self):
        addresses = list(dict.fromkeys(filter(None, [self.desktop().get('DBUS_SESSION_BUS_ADDRESS'),
                                                     f'unix:path=/run/user/{os.getuid()}/bus'])))
        result, mapping = [], {}
        for address in addresses:
            try:
                names = await self.call(address, 'org.freedesktop.DBus', 'org.freedesktop.DBus', 'ListNames')
            except (OSError, ValueError, TimeoutError):
                continue
            for name in names.get('data', [[]])[0]:
                if not name.startswith(PREFIX) or not re.fullmatch(r'[A-Za-z0-9_.-]+', name):
                    continue
                try:
                    data = await self.properties(address, name)
                except (OSError, ValueError, TimeoutError):
                    continue
                token = hashlib.sha256((address + name).encode()).hexdigest()[:24]
                metadata = data.get('Metadata') or {}
                title = metadata.get('xesam:title', {}).get('data') or name[len(PREFIX):]
                artists = metadata.get('xesam:artist', {}).get('data') or []
                control = bool(data.get('CanControl', True))
                playing = data.get('PlaybackStatus') == 'Playing'
                result.append({'id': token, 'name': name[len(PREFIX):], 'title': str(title)[:300],
                    'artist': ', '.join(artists)[:300], 'playing': playing,
                    'can_play_pause': control and bool(data.get('CanPause') if playing else data.get('CanPlay')),
                    'can_next': control and bool(data.get('CanGoNext')),
                    'can_previous': control and bool(data.get('CanGoPrevious')), 'can_seek': control and bool(data.get('CanSeek'))})
                mapping[token] = (address, name)
        self.players = mapping
        result.sort(key=lambda player: (not player['playing'], player['name']))
        return {'players': result}

    async def action(self, player, action):
        if not isinstance(player, str) or not isinstance(action, str) or action not in METHODS:
            raise web.HTTPBadRequest(text='Controle de mídia inválido')
        async with self.lock:
            status = await self.status()
            selected = next((item for item in status['players'] if item['id'] == player), None)
            capability = {'play_pause': 'can_play_pause', 'previous': 'can_previous', 'next': 'can_next', 'seek_back': 'can_seek', 'seek_forward': 'can_seek'}[action]
            if not selected or not selected[capability]:
                raise web.HTTPConflict(text='Este player não permite essa ação no momento')
            address, name = self.players[player]
            try:
                await self.call(address, name, INTERFACE, METHODS[action], *(('x', '-10000000' if action == 'seek_back' else '10000000') if action in ('seek_back', 'seek_forward') else ()))
            except (OSError, ValueError, TimeoutError):
                raise web.HTTPServiceUnavailable(text='O player foi fechado ou não respondeu')
        return {'ok': True}
