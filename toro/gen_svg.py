#!/usr/bin/env python3
"""toro.svg: the RIDER screen's art. Static: the safety mat, the FOLD hose and the turntable. Moving (named nodes,
transformed by toro_screen.c every frame): "shadow" (a unit circle, scaled), "rod" (a unit rectangle mapped onto the
leaning ram) and "rig" (the bull's body, ears and faceplate in rig units: origin on the ram's top, head centre 30
above it, before the rig scale). Horns, eyes, nose ring, steam, sparks, the cowboy and the readouts are drawn by code.
Numbers match toro_screen.c (TR_*). Native subset: one filled contour per path, cubic curves only, <= 4 decimals.
    python3 gen_svg.py > toro.svg
Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE)."""
HK, HY = 1.17, -30
CX0, BASE, S = 150, 132, 0.7


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def h(x, y):                                             # head-local -> rig units
    return (x * HK, y * HK + HY)


def poly(pts):
    return 'M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in pts) + ' Z'


def main():
    o = ['<?xml version="1.0" encoding="utf-8"?>',
         '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="320px" height="160px" viewBox="0 0 320 160">']
    # floor: the mat (a flattened circle as a cubic ellipse), the hose to the FOLD readout and its end stub
    rx, ry, cy = 78, 5.5, BASE + 4
    k = 0.5523
    o.append(f'<path id="mat" fill="none" stroke="#3A3846" stroke-width="1.5" d="M{f(CX0 + rx)} {f(cy)} C{f(CX0 + rx)} {f(cy + k * ry)} {f(CX0 + k * rx)} {f(cy + ry)} {f(CX0)} {f(cy + ry)} '
             f'C{f(CX0 - k * rx)} {f(cy + ry)} {f(CX0 - rx)} {f(cy + k * ry)} {f(CX0 - rx)} {f(cy)} C{f(CX0 - rx)} {f(cy - k * ry)} {f(CX0 - k * rx)} {f(cy - ry)} {f(CX0)} {f(cy - ry)} '
             f'C{f(CX0 + k * rx)} {f(cy - ry)} {f(CX0 + rx)} {f(cy - k * ry)} {f(CX0 + rx)} {f(cy)} Z"/>')
    o.append('<circle id="shadow" fill="#16141D" cx="0" cy="0" r="1"/>')
    o.append(f'<path id="hose" fill="none" stroke="#00ED95" stroke-width="1.5" d="M{CX0 + 20} {BASE + 4} C200 {BASE + 10} 236 {BASE + 6} 256 131"/>')
    o.append('<path id="hosend" fill="none" stroke="#625D77" stroke-width="1.5" d="M256 131 L261 131"/>')
    brx, bry = 26 * S, 4.5 * S
    o.append(f'<path id="base" fill="#24222E" stroke="#B9B9C9" stroke-width="1.5" d="M{f(CX0 + brx)} {BASE} C{f(CX0 + brx)} {f(BASE + k * bry)} {f(CX0 + k * brx)} {f(BASE + bry)} {CX0} {f(BASE + bry)} '
             f'C{f(CX0 - k * brx)} {f(BASE + bry)} {f(CX0 - brx)} {f(BASE + k * bry)} {f(CX0 - brx)} {BASE} C{f(CX0 - brx)} {f(BASE - k * bry)} {f(CX0 - k * brx)} {f(BASE - bry)} {CX0} {f(BASE - bry)} '
             f'C{f(CX0 + k * brx)} {f(BASE - bry)} {f(CX0 + brx)} {f(BASE - k * bry)} {f(CX0 + brx)} {BASE} Z"/>')
    w, ch = 8 * S, 15 * S
    o.append(f'<path id="cyl" fill="#24222E" stroke="#B9B9C9" stroke-width="1.5" d="{poly([(CX0 - w, BASE - ch), (CX0 + w, BASE - ch), (CX0 + w, BASE), (CX0 - w, BASE)])}"/>')
    o.append('<path id="rod" fill="#00ED95" d="M-0.5 0 L0.5 0 L0.5 1 L-0.5 1 Z"/>')
    # the rig, in rig units
    g = ['<g id="rig">']
    g.append('<path fill="#1F1D28" stroke="#B9B9C9" stroke-width="1.5" d="M-36 -40 C-26 -50 26 -50 36 -40 C47 -28 47 -8 34 -2 L-34 -2 C-47 -8 -47 -28 -36 -40 Z"/>')
    for sd in (-1, 1):
        g.append(f'<path fill="none" stroke="#625D77" stroke-width="1.5" d="M{30 * sd} -44 C{40 * sd} -36 {42 * sd} -18 {34 * sd} -4"/>')
    g.append('<path fill="none" stroke="#B9B9C9" stroke-width="2" d="M-6 -52 C-6 -64 6 -64 6 -52"/>')
    for sd in (-1, 1):
        g.append(f'<path fill="#2B2836" stroke="#B9B9C9" stroke-width="1.5" d="{poly([h(x * sd, y) for x, y in [(25, -12), (41, -11), (44, -6), (40, -2), (25, -2)]])}"/>')
    head = [(-20, -22), (20, -22), (27, -10), (24, 4), (17, 16), (14, 28), (8, 32), (-8, 32), (-14, 28), (-17, 16), (-24, 4), (-27, -10)]
    g.append(f'<path fill="#2B2836" stroke="#B9B9C9" stroke-width="2" d="{poly([h(*p) for p in head])}"/>')
    for sd in (-1, 1):
        a, b = h(27 * sd, -10), h(10 * sd, -7)
        g.append(f'<path fill="none" stroke="#625D77" stroke-width="1.5" d="M{f(a[0])} {f(a[1])} L{f(b[0])} {f(b[1])}"/>')
        g.append(f'<path fill="#000000" d="{poly([h(x * sd, y) for x, y in [(21, -9), (8, -5.2), (8, 1.5), (20, -1.8)]])}"/>')
    g.append(f'<path fill="#1B1922" stroke="#B9B9C9" stroke-width="1.5" d="{poly([h(-12, 13), h(12, 13), h(14, 24), h(9, 31), h(-9, 31), h(-14, 24)])}"/>')
    for sd in (-1, 1):
        g.append(f'<path fill="#000000" d="{poly([h(x * sd, y) for x, y in [(3.5, 18), (9, 18), (8, 23), (4, 23)]])}"/>')
    g.append('</g>')
    o += g
    o.append('</svg>')
    print('\n'.join(o))


main()
