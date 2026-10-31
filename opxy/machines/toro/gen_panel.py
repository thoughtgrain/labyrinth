#!/usr/bin/env python3
"""panel.svg.fragment and nodes.h for toro on the OP-XY: the TAURUS screen, a star chart.

The bull is already in the sky. Taurus, drawn the way a chart draws it: points sized by magnitude, hairline joins,
a faint grid of declination arcs, Greek-letter labels, the Pleiades at the shoulder. The controls live on the
stars (display.cpp has the mapping), and comets streak across at random.

Device rules (Open-XY docs/display.md, "Drawing on the device"): every path is one subpath of at most 64 points,
tiny circles are authored as polygons (a curve that small flattens into merged points), lines are closed ribbons.
Each moving part is its own node; display.cpp places, turns, scales and repaints them. Node indices are assigned
here and written to nodes.h.

    python3 gen_panel.py          # writes panel.svg.fragment, nodes.h and the node count into definition.json
Copyright (c) 2026 briannhinton. MIT."""
import json
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLACK, FAINT, GRID, DIM, MID, LITE, WHITE = '#000000', '#2A2A2A', '#161616', '#5A5A5A', '#9A9A9A', '#E6E6E6', '#FFFFFF'
CX, CY = 240, 134                                   # Aldebaran-ish centre of the chart; the asterism sits around it

# the asterism in screen units relative to the chart centre: Aldebaran at the eye, the Hyades V, the two horn stars,
# the back and the tail; and the Pleiades cluster at the shoulder. Names are the chart's.
STARS = {
    'ald': (18, 2, 3.6), 'g1': (-6, -14, 2.1), 'g2': (-26, -24, 1.8), 'g3': (-4, 16, 1.9), 'g4': (-28, 28, 1.7),
    'beta': (110, -56, 2.7), 'zeta': (96, 44, 2.3), 'back': (-70, -6, 1.5), 'tail': (-150, 22, 1.5),
}
JOINS = [('ald', 'g1'), ('g1', 'g2'), ('ald', 'g3'), ('g3', 'g4'), ('g1', 'beta'), ('g3', 'zeta'), ('g2', 'back'), ('g4', 'back'), ('back', 'tail')]
PLEIADES = (-120, -28)


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def pt(p):
    return f'{f(p[0])},{f(p[1])}'


def poly(pts, close=True):
    return 'M' + ' L'.join(pt(p) for p in pts) + (' Z' if close else '')


def dot(r, n=10):
    """a small disc as a polygon: curves this small merge on the device"""
    return poly([(math.cos(k * 2 * math.pi / n) * r, math.sin(k * 2 * math.pi / n) * r) for k in range(n)])


def ring(r, w, n=24):
    """a thin ring as one closed outline: out around, in around, no reversing turn (a small flat join)"""
    out = [(math.cos(k * 2 * math.pi / n) * (r + w / 2), math.sin(k * 2 * math.pi / n) * (r + w / 2)) for k in range(n)]
    inn = [(math.cos(k * 2 * math.pi / n) * (r - w / 2), math.sin(k * 2 * math.pi / n) * (r - w / 2)) for k in range(n)]
    # a ring needs a slit to be one subpath: leave one segment open and join the two circles there
    return poly(out[:-1] + list(reversed(inn[:-1])))


def ribbon(pts, w):
    n = len(pts)
    nx, ny = [], []
    for i in range(n - 1):
        dx, dy = pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]
        ln = math.hypot(dx, dy) or 1
        nx.append(-dy / ln * w / 2); ny.append(dx / ln * w / 2)
    nx.append(nx[-1]); ny.append(ny[-1])
    up = [(x + nx[i], y + ny[i]) for i, (x, y) in enumerate(pts)]
    dn = [(x - nx[i], y - ny[i]) for i, (x, y) in enumerate(pts)]
    return poly(up + list(reversed(dn)))


def spike(r, n=4, inner=0.3):
    """a four-point star for the brightest magnitude"""
    pts = []
    for k in range(n * 2):
        a = -math.pi / 2 + k * math.pi / n
        rr = r if k % 2 == 0 else r * inner
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    return poly(pts)


class Panel:
    def __init__(self):
        self.parts, self.groups = [], []

    def group(self, name, parts):
        first = len(self.parts)
        self.parts += parts
        self.groups.append((name, first, len(parts)))
        return first

    def node(self, name, d, fill, stroke, width):
        return self.group(name, [(d, fill, stroke, width)])

    def nodes_h(self):
        lines = ['#pragma once', '// toro for the OP-XY: node indices, written by gen_panel.py. Edit the generator, not this file.',
                 'namespace toro {', 'enum {']
        for name, first, count in self.groups:
            lines.append(f'    N_{name} = {first}, N_{name}_COUNT = {count},')
        lines.append(f'    N_TOTAL = {len(self.parts)}')
        lines += ['};', '}', '']
        return '\n'.join(lines)


def main():
    P = Panel()
    # ---- paths 0/1: the comet-free sky needs no outlines, but the two slots are reserved; two tiny rings parked ----
    P.node('PATH0', poly([(math.cos(k * 2 * math.pi / 64) * 4, math.sin(k * 2 * math.pi / 64) * 4) for k in range(64)]), None, FAINT, 0.5)
    P.node('PATH1', poly([(math.cos(k * 2 * math.pi / 64) * 4, math.sin(k * 2 * math.pi / 64) * 4) for k in range(64)]), None, FAINT, 0.5)
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    P.group('HDR', [('M0,0 L60,0 L60,20 L0,20 Z', g, g, 0.5) for g in greys])

    # ---- static sky: the grid, the faint field, the Milky Way band, the labels ----
    S = []
    for k in range(1, 5):                                    # declination arcs
        y = 40 + k * 36
        S.append(f'<path d="M0,{y} C160,{y - 10} 320,{y - 10} 480,{y}" fill="none" stroke="{GRID}" stroke-width="0.8"/>')
    for k in range(1, 8):                                    # hour lines
        S.append(f'<path d="M{k * 60},44 L{k * 60 + 8},220" fill="none" stroke="{GRID}" stroke-width="0.8"/>')
    rnd = random.Random(11)
    field = []
    for _ in range(90):                                      # the faint field, static
        x, y = rnd.uniform(4, 476), rnd.uniform(46, 216)
        if abs(x - CX) < 40 and abs(y - CY) < 30:
            continue
        r = rnd.choice([0.7, 0.7, 0.9, 1.2])
        field.append((x, y, r))
        S.append(f'<path d="{poly([(x + math.cos(k * math.pi / 3) * r, y + math.sin(k * math.pi / 3) * r) for k in range(6)])}" fill="{FAINT if r < 1 else DIM}" stroke="none"/>')
    # labels, in the device's own face: Greek letters at the chart's stars (static, near their resting places)
    def label(x, y, s, size=9, anchor='start', fill=DIM):
        S.append(f'<text transform="matrix(1 0 0 1 {f(x)} {f(y)})" fill="{fill}" font-family="UniversNextPro-Light" font-size="{size}px" text-anchor="{anchor}">{s}</text>')
    ax, ay = CX + STARS['ald'][0], CY + STARS['ald'][1]
    label(ax + 9, ay + 3, 'α Aldebaran', 9, 'start', MID)
    label(CX + STARS['beta'][0] + 7, CY + STARS['beta'][1] - 4, 'β', 9)
    label(CX + STARS['zeta'][0] + 7, CY + STARS['zeta'][1] + 10, 'ζ', 9)
    label(CX + STARS['tail'][0] - 2, CY + STARS['tail'][1] + 13, 'ο', 9, 'end')
    label(CX + PLEIADES[0], CY + PLEIADES[1] - 18, 'Pleiades', 8, 'middle')
    label(16, 62, 'TAURUS', 13, 'start', LITE)
    label(464, 212, 'RA 4h 36m  Dec +16° 30′', 8, 'end')

    # ---- the asterism: stars by magnitude (authored at their size, scaled per frame), Aldebaran with a halo ----
    for name, (x, y, r) in STARS.items():
        if name == 'ald':
            P.group('ALD', [(spike(r + 3.2, 4, 0.35), LITE, LITE, 0.3), (dot(r), WHITE, WHITE, 0.3), (ring(r + 4.5, 0.9), DIM, DIM, 0.3)])
        else:
            P.node('S_' + name.upper(), dot(r), LITE, LITE, 0.3)
    # joins: a 100 px ribbon along +x from the first star; placed, turned and scaled to each pair per frame
    P.group('JOIN', [(ribbon([(0, 0), (100, 0)], 0.9), DIM, DIM, 0.3)] * len(JOINS))
    # the Pleiades: seven small stars around the cluster's centre (placed per frame so Res can tighten and turn them)
    pts7 = [(-8, -4, 1.3), (-2, -7, 1.1), (5, -5, 1.4), (9, 1, 1.0), (3, 5, 1.2), (-5, 6, 1.0), (0, 0, 1.6)]
    P.group('PLE', [(dot(r), LITE, LITE, 0.3) for _, _, r in pts7])
    P.group('PLE_RING', [(ribbon([(math.cos(a) * 16, math.sin(a) * 11), (math.cos(a + 0.22) * 16, math.sin(a + 0.22) * 11)], 0.8), FAINT, FAINT, 0.3)
                         for a in [k * math.pi / 5 for k in range(10)]])
    # ---- comets: four, each a head and a tapered tail (authored along -x from the head, 48 px) ----
    tail = poly([(0, -1.6), (-10, -1.3), (-26, -0.8), (-48, -0.2), (-48, 0.2), (-26, 0.8), (-10, 1.3), (0, 1.6)])
    for i in range(4):
        P.group(f'COMET{i}', [(tail, MID, MID, 0.3), (dot(2.0), WHITE, WHITE, 0.3)])
    # ---- twinkle: twelve field stars that can brighten (Noise) ----
    tw = []
    for k in range(12):
        x, y, r = field[k * 7 % len(field)]
        tw.append((dot(1.4), DIM, DIM, 0.3))
    P.group('TWINKLE', tw)
    twinkle_at = [field[k * 7 % len(field)][:2] for k in range(12)]
    # ---- the Milky Way: a dense scatter of faint stars along a band across the top right (static), and one wide
    # faint ribbon under it that Drive brightens ----
    rnd2 = random.Random(23)
    for _ in range(170):
        u = rnd2.random()
        bx = 290 + u * 190; by = 46 + u * 100 + rnd2.gauss(0, 9)
        if by < 46 or by > 216: continue
        r = rnd2.choice([0.6, 0.7, 0.9])
        S.append(f'<path d="{poly([(bx + math.cos(k * math.pi / 3) * r, by + math.sin(k * math.pi / 3) * r) for k in range(6)])}" fill="{FAINT if r < 0.8 else DIM}" stroke="none"/>')
    band = ribbon([(290, 46), (350, 72), (420, 108), (480, 148)], 30)
    P.node('BAND', band, BLACK, BLACK, 0.3)

    # ---- write ----
    o = ['<g id="toro_panel">', '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    def element(i):
        d, fill, stroke, width = P.parts[i]
        x, y = (60 * (i - 2), 0) if 2 <= i < 10 else (640, 320)
        return f'<path id="toro_node_{i}" transform="matrix(1 0 0 1 {x} {y})" d="{d}" fill="{fill if fill else "none"}" stroke="{stroke}" stroke-width="{width}"/>'
    band_index = [first for name, first, count in P.groups if name == 'BAND'][0]
    for i in range(len(P.parts)):
        if i >= 2 and i != band_index:
            o.append(element(i))
        if i == 9:
            for k in range(4):
                o.append(f'<text id="toro_text_{k}" transform="matrix(1 0 0 1 {5 + 120 * k} 10)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{4 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 17.4)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{8 + k}" transform="matrix(1 0 0 1 {5 + 120 * k} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{12 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')
            o.append(element(band_index))                   # the band under the grid and the field
            o += S
    o += [element(0), element(1)]
    solids = ['#000000', '#161616', '#2A2A2A', '#3C3C3C', '#5A5A5A', '#707070', '#8C8C8C', '#9A9A9A', '#B0B0B0', '#C4C4C4', '#D6D6D6', '#E6E6E6', '#F0F0F0', '#F8F8F8', '#FFFFFF', '#FFFFFF']
    for i, s in enumerate(solids):
        o.append(f'<circle id="toro_palette_{i}" cx="640" cy="0" r="1" fill="{s}" stroke="{s}" stroke-width="0.3"/>')
    o.append('</g>')
    (HERE / 'panel.svg.fragment').write_text('\n'.join(o) + '\n', encoding='utf-8')
    h = P.nodes_h()
    # the chart's geometry for display.cpp: star positions, joins, Pleiades offsets and twinkle positions
    h += '\n// chart geometry (pixels relative to the chart centre; twinkle positions absolute)\n'
    h += f'namespace toro {{\nconst int CHART_CX = {CX}, CHART_CY = {CY};\n'
    names = list(STARS.keys())
    h += 'enum { ' + ', '.join('ST_' + n.upper() for n in names) + ', ST_N };\n'
    h += 'const int STAR_X[ST_N] = { ' + ', '.join(str(STARS[n][0]) for n in names) + ' };\n'
    h += 'const int STAR_Y[ST_N] = { ' + ', '.join(str(STARS[n][1]) for n in names) + ' };\n'
    h += 'const int STAR_NODE[ST_N] = { ' + ', '.join(('N_ALD + 1' if n == 'ald' else 'N_S_' + n.upper()) for n in names) + ' };\n'
    h += f'const int JOIN_N = {len(JOINS)};\n'
    h += 'const int JOIN_A[JOIN_N] = { ' + ', '.join('ST_' + a.upper() for a, _ in JOINS) + ' };\n'
    h += 'const int JOIN_B[JOIN_N] = { ' + ', '.join('ST_' + b.upper() for _, b in JOINS) + ' };\n'
    h += f'const int PLE_X = {PLEIADES[0]}, PLE_Y = {PLEIADES[1]};\n'
    h += 'const int PLE_DX[7] = { ' + ', '.join(str(x) for x, _, _ in pts7) + ' };\n'
    h += 'const int PLE_DY[7] = { ' + ', '.join(str(y) for _, y, _ in pts7) + ' };\n'
    h += 'const int TW_X[12] = { ' + ', '.join(str(int(x)) for x, _ in twinkle_at) + ' };\n'
    h += 'const int TW_Y[12] = { ' + ', '.join(str(int(y)) for _, y in twinkle_at) + ' };\n}\n'
    (HERE / 'nodes.h').write_text(h, encoding='utf-8')
    definition = json.loads((HERE / 'definition.json').read_text(encoding='utf-8'))
    definition['resources']['display']['nodes'] = len(P.parts)
    (HERE / 'definition.json').write_text(json.dumps(definition, indent=2) + '\n', encoding='utf-8')
    print(f'{len(P.parts)} nodes in {len(P.groups)} groups')


if __name__ == '__main__':
    main()
