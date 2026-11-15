"""Fixed system actions; updates outlive the phone and gateway process."""
import asyncio
import contextlib
import os
import re
import secrets

from aiohttp import web

ACTIONS = {
    'reboot': ('Reiniciar o JLinux', 'Salve seus arquivos. A conexão será interrompida durante o reinício.'),
    'poweroff': ('Desligar o JLinux', 'Salve seus arquivos. O computador precisará ser ligado novamente para reconectar.'),
    'logout': ('Sair da sessão do JLinux', 'Os aplicativos da sessão serão fechados. Salve seus arquivos antes de continuar.'),
    'update': ('Atualizar o sistema', 'Atualiza os pacotes oficiais. O computador deve permanecer ligado até concluir.'),
}


class SystemControls:
    def __init__(self, desktop):
        self.desktop = desktop
        self.unit = f'jlinux-connect-update-{os.getuid()}.service'
        self.lock = asyncio.Lock()

    async def run(self, *args):
        process = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.STDOUT, env=dict(os.environ, LC_ALL='C'))
        try:
            out, _ = await asyncio.wait_for(process.communicate(), 8)
        except (asyncio.TimeoutError, asyncio.CancelledError):
            with contextlib.suppress(ProcessLookupError):
                process.kill()
            await process.wait()
            raise
        return process.returncode, out.decode(errors='replace')

    async def require(self, *args):
        code, _ = await self.run(*args)
        if code:
            raise web.HTTPForbidden(text='O sistema não autorizou a ação. Verifique as permissões pelo terminal.')

    async def status(self, logs=False):
        _, out = await self.run('/usr/bin/systemctl', 'show', self.unit, '--no-pager',
             '-p', 'LoadState', '-p', 'ActiveState', '-p', 'SubState', '-p', 'Result', '-p', 'ExecMainStatus')
        fields = dict(line.split('=', 1) for line in out.splitlines() if '=' in line)
        if fields.get('LoadState') not in ('loaded', 'not-found'):
            raise web.HTTPServiceUnavailable(text='Não foi possível determinar se uma atualização está em andamento')
        loaded = fields['LoadState'] == 'loaded'
        finished = fields.get('ActiveState') == 'failed' or (
            fields.get('ActiveState') == 'active' and fields.get('SubState') == 'exited')
        # A loaded, queued unit can still be inactive; never stop/reuse it until it has ended.
        running = loaded and not finished
        success = None if not loaded or running else fields.get('Result') == 'success' and fields.get('ExecMainStatus') == '0'
        log = ''
        if logs and loaded:
            code, out = await self.run('/usr/bin/sudo', '-n', '/usr/bin/journalctl', '--unit', self.unit,
                                      '--no-pager', '--output=cat', '--lines=60')
            log = out[-16000:] if code == 0 else 'O registro exige permissão de administrador.'
        return {'running': bool(running), 'success': success, 'log': log,
                'state': 'running' if running else 'idle' if success is None else 'success' if success else 'failed'}

    async def execute(self, action):
        if action not in ACTIONS:
            raise web.HTTPBadRequest(text='Ação inválida')
        async with self.lock:
            update = await self.status()
            if update['running'] and action in ('update', 'reboot', 'poweroff'):
                raise web.HTTPConflict(text='A atualização está em andamento. Aguarde a conclusão antes de atualizar, reiniciar ou desligar.')
            if action == 'update':
                # Retained results are cleared only after verifying that the old job has ended.
                if update['state'] != 'idle':
                    await self.require('/usr/bin/sudo', '-n', '/usr/bin/systemctl', 'stop', self.unit)
                await self.require('/usr/bin/sudo', '-n', '/usr/bin/systemd-run', '--no-block',
                    '--unit=' + self.unit, '--description=Atualização remota do JLinux',
                    '--service-type=exec', '--remain-after-exit', '/usr/bin/systemd-inhibit',
                    '--what=shutdown:sleep', '--mode=block', '--who=JLinux Connect',
                    '--why=Atualização do sistema em andamento', '/usr/bin/pacman', '-Syu', '--noconfirm')
            else:
                if action == 'logout':
                    session = self.desktop().get('XDG_SESSION_ID', '')
                    if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', session):
                        raise web.HTTPServiceUnavailable(text='Nenhuma sessão gráfica identificada para sair.')
                    code, owner = await self.run('/usr/bin/loginctl', 'show-session', session, '--property=User', '--value')
                    if code or owner.strip() != str(os.getuid()):
                        raise web.HTTPForbidden(text='A sessão não pertence ao usuário do JLinux Connect.')
                    command = ['/usr/bin/loginctl', '--no-ask-password', 'terminate-session', session]
                else:
                    command = ['/usr/bin/systemctl', '--no-ask-password', '--check-inhibitors=yes', action]
                # A short system timer lets the phone receive the acknowledgement before disconnecting.
                await self.require('/usr/bin/sudo', '-n', '/usr/bin/systemd-run', '--no-block', '--collect',
                    f'--unit=jlinux-connect-power-{os.getuid()}-{secrets.token_hex(4)}', '--on-active=2s',
                    '--timer-property=AccuracySec=100ms', '--timer-property=RemainAfterElapse=no', *command)
        return {'ok': True, 'action': action}
