"""Only mock fixed OS commands: never reboot, log out or update the real machine."""
import asyncio
import importlib.util
import os
from pathlib import Path
import sys
import threading
import unittest

from aiohttp import web

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from system_controls import SystemControls
from file_transfer import io


class SystemTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.control = SystemControls(lambda: {'XDG_SESSION_ID': 'c9'})
        self.running = False
        self.commands = []
        async def run(*args):
            self.commands.append(args)
            if args[0] == '/usr/bin/systemctl':
                if self.running:
                    return 0, 'LoadState=loaded\nActiveState=active\nSubState=running\nResult=success\nExecMainStatus=0\n'
                return 0, 'LoadState=not-found\nActiveState=inactive\nSubState=dead\n'
            if args[0] == '/usr/bin/loginctl':
                return 0, str(os.getuid())
            return 0, ''
        self.control.run = run

    async def test_only_fixed_actions_and_delayed_power_commands(self):
        with self.assertRaises(web.HTTPBadRequest):
            await self.control.execute('reboot; touch /tmp/bad')
        self.assertEqual(self.commands, [])
        for action in ('reboot', 'poweroff', 'logout'):
            await self.control.execute(action)
            command = self.commands[-1]
            self.assertEqual(command[:4], ('/usr/bin/sudo', '-n', '/usr/bin/systemd-run', '--no-block'))
            self.assertIn('--on-active=2s', command)
            if action == 'logout':
                self.assertEqual(command[-4:], ('/usr/bin/loginctl', '--no-ask-password', 'terminate-session', 'c9'))
            else:
                self.assertEqual(command[-4:], ('/usr/bin/systemctl', '--no-ask-password', '--check-inhibitors=yes', action))

    async def test_update_persists_and_blocks_shutdown_but_allows_logout(self):
        await self.control.execute('update')
        command = self.commands[-1]
        self.assertIn('--remain-after-exit', command)
        self.assertIn('/usr/bin/systemd-inhibit', command)
        self.assertEqual(command[-3:], ('/usr/bin/pacman', '-Syu', '--noconfirm'))
        self.running = True
        for action in ('reboot', 'poweroff', 'update'):
            with self.assertRaises(web.HTTPConflict):
                await self.control.execute(action)
        await self.control.execute('logout')
        self.assertEqual(self.commands[-1][-2:], ('terminate-session', 'c9'))

    async def test_logout_checks_session_owner_and_privilege_errors(self):
        async def run(*args):
            if args[0] == '/usr/bin/systemctl':
                return 0, 'LoadState=not-found\n'
            return 1, 'denied'
        self.control.run = run
        with self.assertRaises(web.HTTPForbidden):
            await self.control.execute('logout')
        with self.assertRaises(web.HTTPForbidden):
            await self.control.execute('reboot')
        self.control.desktop = lambda: {'XDG_SESSION_ID': 'x;reboot'}
        with self.assertRaises(web.HTTPServiceUnavailable):
            await self.control.execute('logout')

    async def test_queued_or_unknown_update_never_allows_shutdown(self):
        async def queued(*args):
            return 0, 'LoadState=loaded\nActiveState=inactive\nSubState=dead\nResult=success\nExecMainStatus=0\n'
        self.control.run = queued
        with self.assertRaises(web.HTTPConflict):
            await self.control.execute('reboot')
        with self.assertRaises(web.HTTPConflict):
            await self.control.execute('update')
        async def broken(*args):
            return 1, 'Unable to connect to system bus'
        self.control.run = broken
        with self.assertRaises(web.HTTPServiceUnavailable):
            await self.control.execute('poweroff')

    async def test_cancel_waits_for_file_worker_before_fd_cleanup(self):
        entered, release = threading.Event(), threading.Event()
        def work():
            entered.set()
            release.wait(2)
        task = asyncio.create_task(io(work))
        while not entered.is_set():
            await asyncio.sleep(.005)
        task.cancel()
        await asyncio.sleep(.01)
        self.assertFalse(task.done())
        release.set()
        with self.assertRaises(asyncio.CancelledError):
            await task
