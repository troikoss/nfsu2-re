#!/usr/bin/env python3
"""Native forward/reverse integration checks, not original trajectory matching."""
import math
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/analysis/demo-recovery'
ROW = re.compile(r't=\s*([\d.]+) pos=\(\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\) '
                 r'speed=\s*([-\d.]+) km/h rpm=\s*([-\d.]+) gear=(\d+)')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, frames in (('skyline', 960), ('reverse', 360)):
        env = dict(os.environ)
        for key in ('UG2_REVERSE', 'UG2_STEER'):
            env.pop(key, None)
        if name == 'reverse':
            env['UG2_REVERSE'] = '1'
        result = subprocess.run([str(ROOT / 'build/native/ug2sim'),
                                 'Need for Speed Underground 2', 'SKYLINE', str(frames), '--no-mods'],
                                cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
        log = result.stdout + result.stderr
        (OUT / (name + '-smoke.log')).write_text(log)
        assert result.returncode == 0, (name, result.returncode, result.stderr)
        assert 'unrecovered original code' not in log, name
        rows = [tuple(float(value) for value in row) for row in ROW.findall(log)]
        assert len(rows) == frames // 30 + 1, (name, len(rows))
        assert all(math.isfinite(value) for row in rows for value in row), name
        gears = {int(row[6]) for row in rows}
        maximum = max(row[4] for row in rows)
        assert maximum > (30 if name == 'skyline' else 12), (name, maximum)
        displacement = rows[-1][1] - rows[0][1]
        # The original L4RA start grid (TrackPosMarkersFreeRoam.bin slot 0) sits on
        # parking bays facing +X: a kerb about 20 m ahead and a planter about 5 m behind. Check movement direction as well as ratio
        # selection; a finite idle/neutral run must not pass this check.
        if name == 'skyline':
            assert 2 in gears and displacement > 15, (gears, displacement)
        else:
            assert 0 in gears and displacement < -3, (gears, displacement)
        print('PASS %s: %d samples, %.1f km/h maximum, gears %s' %
              (name, len(rows), maximum, sorted(gears)))


if __name__ == '__main__':
    main()
