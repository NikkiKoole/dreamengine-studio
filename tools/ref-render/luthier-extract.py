#!/usr/bin/env python3
# Extract chrisjz/luthier's physics core out of its single index.html.
#
# The whole FDTD string lives in a JS array of string literals (`var PHYS = [ "...", ... ]`)
# which the page itself feeds to `new Function(...)` to run on the main thread. That is why this
# is a text extraction and not a port: the code that comes out is the code that makes the sound,
# so a reference render cannot drift from what the page does.
#
#   python3 luthier-extract.py <index.html> <out phys.js> <out tune.js>
import re, io, json, sys
src, out_phys, out_tune = sys.argv[1], sys.argv[2], sys.argv[3]
s = io.open(src, encoding='utf-8').read()
i = s.index('var PHYS = [')
j = s.index("].join('\\n');", i)
block = s[i + len('var PHYS = ['):j]
lines = [json.loads('"' + m.group(1) + '"')
         for m in re.finditer(r'"((?:[^"\\]|\\.)*)"\s*,?\s*\n', block + '\n')]
io.open(out_phys, 'w', encoding='utf-8').write('\n'.join(lines))
# the self-tuning half (fftMag .. tuneString) is plain page script, not in the array
a = s.index('function fftMag(re)'); b = s.index('/* ============================ notes ===')
io.open(out_tune, 'w', encoding='utf-8').write(s[a:b])
print('%s: %d lines  ·  %s' % (out_phys, len(lines), out_tune))
