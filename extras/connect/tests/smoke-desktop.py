#!/usr/bin/env python3
"""Capture/input integration on a private compositor and an isolated mobile browser."""
import asyncio
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile

from aiohttp import web

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('connect_server', ROOT / 'server.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


async def main():
    with tempfile.TemporaryDirectory(prefix='jlinux-connect-desktop-') as directory:
        root = Path(directory)
        env = dict(os.environ, XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=str(root / 'config'),
                   WLR_BACKENDS='headless', WLR_RENDERER='pixman', WLR_HEADLESS_OUTPUTS='1',
                   JLINUX_REMOTE_CONTROL='1')
        processes = []
        runner = None
        with (root / 'session.log').open('w') as log:
            try:
                compositor = await asyncio.create_subprocess_exec(str(Path(sys.argv[1]).resolve()), env=env, stdout=log, stderr=log)
                processes.append(compositor)
                async with asyncio.timeout(8):
                    while 'JLinux compositor pronto:' not in (root / 'session.log').read_text():
                        assert compositor.returncode is None
                        await asyncio.sleep(.05)
                env['WAYLAND_DISPLAY'] = next(p.name for p in root.glob('wayland-*') if p.is_socket())
                foot = await asyncio.create_subprocess_exec('foot', '--config=/dev/null', '--title=Connect isolated fixture',
                       'bash', '--noprofile', '--norc', env=env, stdout=log, stderr=log)
                processes.append(foot)
                async with asyncio.timeout(5):
                    while 'Janela mapeada: Connect isolated fixture' not in (root / 'session.log').read_text():
                        await asyncio.sleep(.05)
                companion = module.Companion(root / 'state', ROOT / 'web', desktop=lambda: env)
                level = .42
                async def command(*args):
                    nonlocal level
                    if 'set-volume' in args:
                        level = float(args[-1])
                    return f'Volume: {level}\n'
                companion.command = command
                runner = web.AppRunner(companion.application(), access_log=None)
                await runner.setup()
                site = web.TCPSite(runner, '127.0.0.1', 0)
                await site.start()
                port = site._server.sockets[0].getsockname()[1]
                companion.hosts.add(f'127.0.0.1:{port}')
                fixture = root / 'fixture.json'
                fixture.write_text(json.dumps({'url': f'http://127.0.0.1:{port}', 'key': companion.key,
                                               'marker': str(root / 'remote-keyboard-marker')}))
                browser = await asyncio.create_subprocess_exec('node', str(ROOT / 'tests/smoke-mobile.mjs'), str(fixture))
                processes.append(browser)
                assert await asyncio.wait_for(browser.wait(), 55) == 0, 'Mobile browser integration failed'
            except Exception:
                print((root / 'session.log').read_text(), file=sys.stderr)
                raise
            finally:
                if runner:
                    await runner.cleanup()
                for process in reversed(processes):
                    if process.returncode is None:
                        with contextlib.suppress(ProcessLookupError):
                            process.terminate()
                        await process.wait()


asyncio.run(main())
