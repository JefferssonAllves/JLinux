#!/usr/bin/env python3
"""Convert the bundled AdAway hosts snapshot to native WebKit rules (development only)."""
import json
from pathlib import Path
import re
import sys


def rules_from_hosts(text):
    domains = set()
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        address, host = line.split()
        if host == 'localhost':
            continue
        if address not in ('127.0.0.1', '0.0.0.0') or len(host) > 253 or not all(
                re.fullmatch(r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?', label) for label in host.split('.')):
            raise ValueError(f'Invalid hosts entry: {line}')
        domains.add(host)
    if not domains:
        raise ValueError('Empty domain list')
    rules = []
    for host in sorted(domains):
        parts = host.split('.')
        if any('.'.join(parts[i:]) in domains for i in range(1, len(parts))):
            continue
        pattern = r'^https?://([^/:]+\.)?' + host.replace('.', r'\.') + r'[/:]'
        rules.append({'trigger': {'url-filter': pattern, 'load-type': ['third-party']}, 'action': {'type': 'block'}})
    rules.append({'trigger': {'url-filter': r'^https?://[^/]+/pagead/', 'resource-type': ['script', 'image', 'raw']},
                  'action': {'type': 'block'}})
    rules.append({'trigger': {'url-filter': '.*'}, 'action': {'type': 'css-display-none',
                  'selector': 'ins.adsbygoogle, [data-ad-client], iframe[id^="google_ads_iframe"]'}})
    return rules


if __name__ == '__main__':
    source, destination = map(Path, sys.argv[1:])
    rules = rules_from_hosts(source.read_text())
    destination.write_text(json.dumps(rules, separators=(',', ':')) + '\n')
    print(f'{len(rules)} WebKit rules')
