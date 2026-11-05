#!/usr/bin/env python3
"""Render independent effect combinations and reject invalid preferences."""
import hashlib
import itertools
import os
from pathlib import Path
import subprocess
import sys
import tempfile

names = ('JLINUX_GLASS_BLUR', 'JLINUX_GLASS_TINT', 'JLINUX_GLASS_REFLECTION')
with tempfile.TemporaryDirectory(prefix='jlinux-glass-test-') as directory:
    output = Path(directory) / 'preview.png'
    hashes = set()
    for values in itertools.product(('0', '1'), repeat=3):
        env = dict(os.environ, **dict(zip(names, values)))
        subprocess.run([sys.argv[1], '--preview', str(output)], env=env, check=True)
        hashes.add(hashlib.sha256(output.read_bytes()).digest())
    if len(hashes) != 8:
        raise RuntimeError('Effect combinations did not produce distinct images')
    for name in (*names, 'JLINUX_GLASS_SHADOW'):
        invalid_env = dict(env, **{name: 'invalid'})
        result = subprocess.run([sys.argv[1], '--preview', str(output)], env=invalid_env)
        if result.returncode != 1:
            raise RuntimeError('Invalid effect preference was accepted: ' + name)
