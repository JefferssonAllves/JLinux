import asyncio
from pathlib import Path
import socket
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock, patch
from aiohttp import web
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from remote_controls import RemoteControls, KEYS


class RemoteTests(unittest.IsolatedAsyncioTestCase):
    async def test_dock_uses_fixed_native_indices_and_private_socket(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            control=socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM)
            self.addCleanup(control.close)
            control.bind(str(root/'jlinux-window-control'));control.settimeout(1)
            remote=RemoteControls(lambda:{'XDG_RUNTIME_DIR':temp},data_dirs=[root])
            remote.launch('7')
            self.assertEqual(control.recv(2),b'\x07')
            for bad in ('4','10','-1','../0','00',0,False):
                with self.assertRaises(web.HTTPBadRequest):remote.launch(bad)
            root.chmod(0o755)
            with self.assertRaises(web.HTTPServiceUnavailable):remote.launch('0')

    async def test_navigation_only_sends_named_keys_without_a_shell(self):
        process=AsyncMock();process.returncode=0;process.wait.return_value=0
        remote=RemoteControls(lambda:{'WAYLAND_DISPLAY':'fixture'})
        with patch('remote_controls.asyncio.create_subprocess_exec',AsyncMock(return_value=process)) as spawn:
            for action,args in KEYS.items():
                await remote.key(action)
                self.assertEqual(spawn.call_args.args,('wtype',*args))
            count=spawn.call_count
            for bad in ('rm -rf','Return','',{},None):
                with self.assertRaises(web.HTTPBadRequest):await remote.key(bad)
            self.assertEqual(spawn.call_count,count)

    async def test_icons_use_installed_metadata_and_have_a_safe_fallback(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'applications').mkdir()
            icon=root/'brave.svg';icon.write_text('<svg xmlns="http://www.w3.org/2000/svg"/>')
            (root/'applications/brave-browser.desktop').write_text('[Desktop Entry]\nIcon='+str(icon)+'\n')
            remote=RemoteControls(lambda:{},data_dirs=[root])
            self.assertEqual(remote.icon(0),(icon.read_bytes(),'image/svg+xml'))
            self.assertIn(b'<svg',remote.icon(1)[0])
            self.assertEqual([app['id'] for app in remote.apps()],['0','1','2','3','5','6','7','8','9'])
