#!/usr/bin/env python3
"""Check a panel.svg.fragment against the OP-XY's drawing rules.

These are the hardware findings in the Open-XY SDK's docs/display.md, "Drawing on the device" (PR 260), checked
on the authored artwork before it goes anywhere near a device:

  - every path is one subpath of at most 64 points (compound paths stopped the device at launch)
  - no turn within 2.5 degrees of straight back (a near-reversal draws a streak); 180-degree turns are the usual
    case, from a line traced out and back
  - neighbouring points at least 1/8 px apart (closer points merge and the outline stops reshaping)
  - the closing edge, from the last point back to the first, at least 1/4 px (shorter draws a sliver)

Curves are flattened for the angle checks. Paths the SDK prescribes (the player keyboards' H/V rectangles) are
parsed with H and V. Exit status is the number of findings.

    python3 tools/panel_lint.py machines/*/panel.svg.fragment
Copyright (c) 2026 briannhinton. MIT."""
import math
import re
import sys
import xml.etree.ElementTree as ET

NUMBER = r'-?\d*\.?\d+(?:e-?\d+)?'
TOKEN = re.compile(r'([MLHVCZmlhvcz])|(' + NUMBER + ')')


def flatten(d):
    """A path's subpaths as polylines (absolute commands only, as the SDK requires), curves sampled at 6 points."""
    subpaths, pts, cmd, buf, cur = [], [], None, [], None
    for c, n in TOKEN.findall(d):
        if c:
            if c in 'Zz' and pts:
                pts.append(('Z', pts[0][1]))
            if c in 'Mm' and pts:
                subpaths.append(pts); pts = []
            cmd, buf = c, []
            continue
        buf.append(float(n))
        if cmd == 'M' and len(buf) == 2: cur = (buf[0], buf[1]); pts.append(('M', cur)); buf = []
        elif cmd == 'L' and len(buf) == 2: cur = (buf[0], buf[1]); pts.append(('L', cur)); buf = []
        elif cmd == 'H' and len(buf) == 1: cur = (buf[0], cur[1]); pts.append(('L', cur)); buf = []
        elif cmd == 'V' and len(buf) == 1: cur = (cur[0], buf[0]); pts.append(('L', cur)); buf = []
        elif cmd == 'C' and len(buf) == 6:
            p0, p1, p2, p3 = cur, (buf[0], buf[1]), (buf[2], buf[3]), (buf[4], buf[5])
            for k in range(1, 7):
                u = k / 6; v = 1 - u
                pts.append(('C', (v ** 3 * p0[0] + 3 * v * v * u * p1[0] + 3 * v * u * u * p2[0] + u ** 3 * p3[0],
                                  v ** 3 * p0[1] + 3 * v * v * u * p1[1] + 3 * v * u * u * p2[1] + u ** 3 * p3[1])))
            cur, buf = p3, []
    if pts:
        subpaths.append(pts)
    return subpaths


def coordinate_pairs(d):
    return len(re.findall(NUMBER, d)) // 2


def check_polyline(poly, name, out):
    xy = [p for _, p in poly]
    # drop the closing duplicate for the angle walk, but remember it for the closing edge
    closed = poly[-1][0] == 'Z'
    if closed:
        xy = xy[:-1]
    if closed and len(xy) > 1 and xy[-1] == xy[0]:
        xy = xy[:-1]                                     # a curve that ends exactly where it began: the device merges the pair
    n = len(xy)
    if n < 3:
        return
    # the closing edge
    if closed:
        cx, cy = xy[-1][0] - xy[0][0], xy[-1][1] - xy[0][1]
        if 0 < math.hypot(cx, cy) < 0.25:
            out.append(f'{name}: closing edge {math.hypot(cx, cy):.3f} px (under 1/4 px)')
    ring = xy + ([xy[0], xy[1]] if closed else [])
    for k in range(1, len(ring) - 1):
        ax, ay = ring[k][0] - ring[k - 1][0], ring[k][1] - ring[k - 1][1]
        bx, by = ring[k + 1][0] - ring[k][0], ring[k + 1][1] - ring[k][1]
        la, lb = math.hypot(ax, ay), math.hypot(bx, by)
        if la == 0 or lb == 0:
            out.append(f'{name}: repeated point near {ring[k]}')
            return
        if la < 0.125:
            out.append(f'{name}: points {la:.3f} px apart near {ring[k]} (under 1/8 px)')
            return
        cosine = max(-1.0, min(1.0, (ax * bx + ay * by) / (la * lb)))
        turn = math.degrees(math.acos(cosine))
        if turn > 177.5:
            out.append(f'{name}: turn of {turn:.1f} degrees (within 2.5 of straight back) near {ring[k]}')
            return


def check(path):
    out = []
    root = ET.fromstring(open(path, encoding='utf-8').read())
    for el in root.iter():
        if el.tag.split('}')[-1] != 'path':
            continue
        d = el.get('d', '')
        name = el.get('id') or '(static path)'
        subpaths = flatten(d)
        if len(subpaths) > 1:
            out.append(f'{name}: {len(subpaths)} subpaths (one allowed)')
        pairs = coordinate_pairs(d)
        if pairs > 64:
            out.append(f'{name}: {pairs} coordinate pairs (64 allowed)')
        for poly in subpaths:
            check_polyline(poly, name, out)
    return out


def main(argv):
    total = 0
    for path in argv:
        findings = check(path)
        for f in findings:
            print(f'{path}: {f}')
        print(f'{path}: {len(findings)} finding{"s" * (len(findings) != 1)}')
        total += len(findings)
    return min(total, 255)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
