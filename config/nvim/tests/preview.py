import http.client
import os
import subprocess
import urllib.parse
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest

spec = importlib.util.spec_from_file_location('live_preview', Path(__file__).resolve().parents[1]/'preview.py')
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


class PreviewTests(unittest.TestCase):
    def test_html_assets_reload_and_local_boundaries(self):
        with tempfile.TemporaryDirectory(prefix='html live ') as temp:
            root = Path(temp)
            html = root/'page with space.html'
            html.write_text('<link rel="stylesheet" href="style.css"><body>original</body>')
            css = root/'style.css'; css.write_text('body{color:red}')
            server = preview.Preview(html)
            worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
            host = '127.0.0.1:'+str(server.server_port)
            client = http.client.HTTPConnection(host, timeout=4)
            events = http.client.HTTPConnection(host, timeout=4)
            try:
                def get(path, method='GET', headers=None):
                    client.request(method, path, headers=headers or {})
                    response = client.getresponse(); content = response.read()
                    return response, content
                response, content = get('/page%20with%20space.html')
                self.assertEqual(response.status,200)
                self.assertIn(b'EventSource',content)
                self.assertIn(b'</script></body>',content)
                self.assertNotIn('EventSource',html.read_text())
                self.assertEqual(response.getheader('Cache-Control'),'no-store')
                self.assertEqual(get('/style.css')[1],css.read_bytes())
                events.request('GET','/__nvim_live_events')
                stream = events.getresponse()
                self.assertEqual(stream.readline().strip(),b'data: 0'); stream.readline()
                html.write_text('<body>updated</body>')
                first = stream.readline().strip(); stream.readline()
                self.assertNotEqual(first,b'data: 0')
                css.write_text('body{color:blue}')
                second = stream.readline().strip(); stream.readline()
                self.assertNotEqual(second,first)
                response, content = get('/page%20with%20space.html', headers={'If-Modified-Since':'Wed, 01 Jan 2031 00:00:00 GMT'})
                self.assertEqual(response.status,200)
                self.assertIn(b'updated',content)
                (root/'outside').symlink_to(root.parent, target_is_directory=True)
                self.assertEqual(get('/outside/private.txt')[0].status,403)
                self.assertEqual(get('/outside/private.txt','HEAD')[0].status,403)
                folder=root/'folder';folder.mkdir();(folder/'index.htm').symlink_to(Path(__file__).resolve())
                self.assertEqual(get('/folder/')[0].status,403)
                self.assertEqual(get('/style.css',headers={'Host':'evil.invalid'})[0].status,403)
                self.assertEqual(server.server_address[0],'127.0.0.1')
            finally:
                client.close();events.close();server.shutdown();server.server_close();worker.join()

    @unittest.skipUnless(os.environ.get('JLINUX_PLAYWRIGHT_MODULE'), 'Playwright module not selected')
    def test_real_browser_reload(self):
        with tempfile.TemporaryDirectory(prefix='browser live ') as temp:
            root=Path(temp);html=root/'index with space.html';css=root/'style.css';script=root/'app.js'
            html.write_text('<link rel="stylesheet" href="style.css"><h1>original</h1><script src="app.js"></script>')
            css.write_text('h1{color:red}');script.write_text('document.body.dataset.live="original"')
            server=preview.Preview(html)
            worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
            try:
                url=f'http://127.0.0.1:{server.server_port}/'+urllib.parse.quote(html.name)
                subprocess.run(['node',str(Path(__file__).with_name('live-browser.mjs')),url,str(html),str(css),str(script)],check=True,timeout=25)
            finally:
                server.shutdown();server.server_close();worker.join()


if __name__ == '__main__':
    unittest.main()
