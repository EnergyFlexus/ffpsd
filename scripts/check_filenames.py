#!/usr/bin/env python3
import re
import sys
from pathlib import Path

PATTERN = re.compile(r'^[a-z][a-z0-9_]*\.(h|hpp|c|cpp)$')
C_READABLE = {'export', 'c_api'}

bad = []
for arg in sys.argv[1:]:
    p = Path(arg)
    if not PATTERN.match(p.name):
        bad.append(f'{arg}: expected lowercase_with_underscores')
        continue
    if p.suffix == '.hpp' and p.stem in C_READABLE:
        bad.append(f'{arg}: C-readable header must use .h')

if bad:
    print('\n'.join(bad), file=sys.stderr)
    sys.exit(1)