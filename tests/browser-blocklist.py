#!/usr/bin/env python3
"""Check snapshot conversion, host boundaries and rejection of malformed source entries."""
import json
from pathlib import Path
import re
import runpy

root = Path(__file__).resolve().parents[1]
convert = runpy.run_path(str(root / 'scripts/generate-browser-blocklist.py'))['rules_from_hosts']
rules = convert('127.0.0.1 localhost\n::1 localhost\n0.0.0.0 ad.example\n127.0.0.1 sub.ad.example\n')
assert len(rules) == 3, rules
pattern = rules[0]['trigger']['url-filter']
for url in ['https://ad.example/a', 'http://sub.ad.example:8080/a']:
    assert re.search(pattern, url), url
for url in ['https://notad.example/a', 'https://ad.example.evil/a', 'https://safe.example/ad.example/a']:
    assert not re.search(pattern, url), url
for text in ['', '0.0.0.0 *.example', '0.0.0.0 a..example', '8.8.8.8 ad.example', '0.0.0.0 https://ad.example']:
    try:
        convert(text)
    except ValueError:
        pass
    else:
        raise AssertionError(text)
assert convert((root / 'data/browser/adaway-hosts.txt').read_text()) == json.loads((root / 'data/browser/blocker.json').read_text())
print('Blocklist: reproducible snapshot, domain boundaries and invalid input passed.')
