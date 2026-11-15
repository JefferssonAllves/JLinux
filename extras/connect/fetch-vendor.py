#!/usr/bin/env python3
"""Fetch only the pinned noVNC browser modules, checking npm's recorded SHA-512."""
import base64
import hashlib
import io
from pathlib import Path
import tarfile
import urllib.request

VERSION = '1.7.0'
INTEGRITY = 'ucEJOx4T2avIRCleodk7YobZj5O2Ga2AeLfQ69A/yjG9HHba2+PDgwSkN3FttrmG+70ZGx21sElNFouK13RzyA=='
blob = urllib.request.urlopen(f'https://registry.npmjs.org/@novnc/novnc/-/novnc-{VERSION}.tgz', timeout=30).read()
if base64.b64encode(hashlib.sha512(blob).digest()).decode() != INTEGRITY:
    raise SystemExit('noVNC integrity check failed')
root = Path(__file__).resolve().parent / 'web/vendor/novnc'
with tarfile.open(fileobj=io.BytesIO(blob), mode='r:gz') as archive:
    for item in archive:
        relative = Path(item.name).relative_to('package')
        if (not item.isfile() or '..' in relative.parts or
            relative.parts[0] not in ('core', 'vendor', 'LICENSE.txt', 'AUTHORS', 'package.json') and not str(relative).startswith('docs/LICENSE.')):
            continue
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(archive.extractfile(item).read())
print('noVNC 1.7.0 verified and installed')
