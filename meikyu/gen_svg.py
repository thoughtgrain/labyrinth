#!/usr/bin/env python3
"""meikyu.svg: the THREAD screen's static art (walls of the round labyrinth and the bull at its centre).
The numbers match meikyu_screen.c (TH_*): centre (160, 74); walls at radii 61, 49, 37, 25, 13, each open where the
path drops a ring. The path starts at the bottom (a quarter turn) and each ring runs 4 steps of 0.86 quarter turns,
alternating direction. Native subset: one contour per path, M/L only for the arcs, at most 4 decimals.
    python3 gen_svg.py > meikyu.svg
Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE)."""
import math

CX, CY, WALLS, SP = 160, 74, [61, 49, 37, 25, 13], math.pi / 2 * 1761 / 2048
DECOR, SEC = '#625D77', '#B9B9C9'


def f(v):
    s = f'{v:.2f}'.rstrip('0').rstrip('.')
    return '0' if s == '-0' else s


def main():
    start = [math.pi / 2]
    for r in range(1, 5):
        start.append(start[-1] + (1 if r % 2 else -1) * 4 * SP)
    out = ['<?xml version="1.0" encoding="utf-8"?>',
           '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="320px" height="160px" viewBox="0 0 320 160">']
    for w, r in enumerate(WALLS):
        ga, n = 7 / r, 40 if r > 30 else 24
        a0, a1 = start[w] + ga, start[w] + 2 * math.pi - ga
        pts = [(CX + r * math.cos(a0 + (a1 - a0) * i / n), CY + r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]
        d = 'M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in pts)
        out.append(f'<path id="wall{w}" fill="none" stroke="{DECOR}" stroke-width="1.5" d="{d}"/>')
    # the bull at the centre: two horns and a head
    out.append(f'<path id="hornl" fill="none" stroke="{SEC}" stroke-width="1.5" d="M{CX - 5} {CY + 2} C{CX - 8} {CY - 3} {CX - 6} {CY - 6} {f(CX - 2.5)} {f(CY - 4.5)}"/>')
    out.append(f'<path id="hornr" fill="none" stroke="{SEC}" stroke-width="1.5" d="M{CX + 5} {CY + 2} C{CX + 8} {CY - 3} {CX + 6} {CY - 6} {f(CX + 2.5)} {f(CY - 4.5)}"/>')
    out.append(f'<circle id="head" fill="none" stroke="{SEC}" stroke-width="1.5" cx="{CX}" cy="{CY + 1}" r="2.6"/>')
    out.append('</svg>')
    print('\n'.join(out))


main()
