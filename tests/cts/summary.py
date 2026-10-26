#!/usr/bin/env python3
"""summary.py <results.csv> [depth]: status counts overall and failing cases grouped by prefix."""
import csv, sys, collections
rows = list(csv.reader(open(sys.argv[1])))
depth = int(sys.argv[2]) if len(sys.argv) > 2 else 4
tot = collections.Counter(r[1] for r in rows)
print("total", len(rows), dict(tot))
bad = collections.defaultdict(collections.Counter)
for c, s, *_ in rows:
    if s not in ("Pass", "NotSupported", "QualityWarning", "CompatibilityWarning"):
        bad[".".join(c.split(".")[:depth])][s] += 1
for g, cnt in sorted(bad.items(), key=lambda x: -sum(x[1].values())):
    print(f"{sum(cnt.values()):6d} {g} {dict(cnt)}")
