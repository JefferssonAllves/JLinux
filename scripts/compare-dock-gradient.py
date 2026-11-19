#!/usr/bin/env python3
"""Compare fixed, icon-free material sample points; not a whole-image fidelity score."""
import json
import math
from pathlib import Path
import sys
from PIL import Image
reference, native = [Image.open(path).convert('RGB') for path in sys.argv[1:3]]
if reference.size != native.size or reference.size != (1254, 1254):
    raise SystemExit('As duas imagens devem ter 1254×1254.')
samples = []
for sector in range(9):  # Selected sector 9 is excluded because its animated glow varies.
    angle = math.radians(-90 + sector * 36)
    for radius in (330, 560):
        point = (round(627 + radius * math.cos(angle)), round(627 + radius * math.sin(angle)))
        samples.append(dict(sector=sector, point=point, reference=reference.getpixel(point), native=native.getpixel(point)))
error = sum(abs(a-b) for s in samples for a,b in zip(s['reference'], s['native'])) / (3*len(samples))
print(json.dumps(dict(reference=Path(sys.argv[1]).name, native=Path(sys.argv[2]).name,
    scope='18 fixed material points, excluding icons and selected sector; not full-image equivalence',
    sample_mae_rgb=round(error, 3), samples=samples), indent=2))
