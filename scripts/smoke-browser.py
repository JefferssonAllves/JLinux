#!/usr/bin/env python3
"""Listing fixtures include symlinks and byte names; never touches real user data."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='jlinux-browser-') as directory:
    root = Path(directory)
    (root / 'Folder').mkdir()
    (root / 'folder-link').symlink_to('Folder', target_is_directory=True)
    (root / 'broken-link').symlink_to('missing')
    (root / 'sample.txt').write_text('example')
    (root / 'name with spaces.txt').write_text('example')
    (root / '.hidden').write_text('example')
    fd = os.open(os.fsencode(root) + b'/invalid-\xff', os.O_WRONLY | os.O_CREAT, 0o600)
    os.close(fd)
    subprocess.run([sys.argv[1], directory], check=True)
