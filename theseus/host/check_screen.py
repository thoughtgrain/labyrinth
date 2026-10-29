#!/usr/bin/env python3
"""Rule check over the host preview frames (meikyu or toro host/screen_host output):
  - artwork ink inside the safe rectangle 8..312 x 8..144
  - readout ink (text and icons) inside 8..312 x 8..140
  - every artwork stroke at least 2 units (ink to ink, stroke widths included) from every readout stroke
    python3 check_screen.py DIR
Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE)."""
import glob, math, re, sys

LINE = re.compile(r'<line class="(\w+)" x1="([-\d.]+)" y1="([-\d.]+)" x2="([-\d.]+)" y2="([-\d.]+)" stroke="[^"]+" stroke-width="([\d.]+)"')


def pts(x1, y1, x2, y2):
    n = max(1, int(math.hypot(x2 - x1, y2 - y1) / 0.75))
    return [(x1 + (x2 - x1) * i / n, y1 + (y2 - y1) * i / n) for i in range(n + 1)]


def main():
    d = sys.argv[1]
    fails, closest = 0, 1e9
    files = sorted(glob.glob(d + '/*.svg'))
    for f in files:
        ro, art = [], []
        for cls, *v in LINE.findall(open(f).read()):
            x1, y1, x2, y2, w = map(float, v)
            (ro if cls.startswith('ro') else art).append((x1, y1, x2, y2, w))
        errs = []
        for x1, y1, x2, y2, w in ro:
            hw = w / 2
            if min(x1, x2) - hw < 8 or max(x1, x2) + hw > 312 or min(y1, y2) - hw < 8 or max(y1, y2) + hw > 140.05:
                errs.append(f'readout ink outside 8..312 x 8..140: {x1:.1f},{y1:.1f} {x2:.1f},{y2:.1f}'); break
        for x1, y1, x2, y2, w in art:
            hw = w / 2
            if min(x1, x2) - hw < 8 or max(x1, x2) + hw > 312 or min(y1, y2) - hw < 8 or max(y1, y2) + hw > 144:
                errs.append(f'art outside safe area: {x1:.1f},{y1:.1f} {x2:.1f},{y2:.1f}'); break
        # readout points in a coarse grid for speed
        grid = {}
        for x1, y1, x2, y2, w in ro:
            for x, y in pts(x1, y1, x2, y2):
                grid.setdefault((int(x // 8), int(y // 8)), []).append((x, y, w / 2))
        for x1, y1, x2, y2, w in art:
            for x, y in pts(x1, y1, x2, y2):
                gx, gy = int(x // 8), int(y // 8)
                for i in (-1, 0, 1):
                    for j in (-1, 0, 1):
                        for rx, ry, rhw in grid.get((gx + i, gy + j), ()):
                            g = math.hypot(x - rx, y - ry) - rhw - w / 2
                            if g < closest: closest = g
                            if g < 2 and len(errs) < 3:
                                errs.append(f'art within 2 of readout ink near {x:.1f},{y:.1f}')
        if errs:
            fails += 1
            if fails <= 6: print('FAIL', f.split('/')[-1], '\n   ' + '\n   '.join(errs[:3]))
    print(f'{len(files)} frames, {fails} failing, closest artwork-to-readout ink gap {closest:.1f}')
    sys.exit(1 if fails else 0)


main()
