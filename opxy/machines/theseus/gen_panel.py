#!/usr/bin/env python3
"""panel.svg.fragment and nodes.h for theseus on the OP-XY: the TOWER screen, the kit stacked as a drum tower.

Theseus is one voice that plays the whole kit; the note picks the drum. So the screen is the kit, stacked from the
kick at the base to the hi-hat on top, each tier drawn as the drum it plays (the kick's wrap stripe and beater patch,
the floor tom's legs, the snare's wires, the rack tom's mount, the hi-hat's rod). It's a line drawing: black shapes
with grey outlines, so five overlapping drums stay separate. Only the drum that's hit moves, only upward, and only
its head fills with light. The floor under the kick is the hit LFO's clock. display.cpp has the control mapping.

How it's cut for the device (Open-XY docs/display.md, "Artwork"):
  - Every tier is a group of nodes sharing one origin, the centre of its head, so a hit lifts the tier with one
    translation per node and nothing can come apart. Rings and overtone hoops are per tier too, authored round that
    same origin at full size and scaled.
  - The hi-hat's two cymbals each share a pivot, the cymbal's own centre, so display.cpp can rock them with a turn.
  - Lines are closed ribbons, hoops are C-shaped ribbons with a hairline gap (every point distinct, no repeated
    seam point), and every path is one subpath of at most 64 points.
  - The floor clock is pre-drawn for each of the ten Rate settings (2..32 dashes, 121 nodes); only the set for the
    current Rate is shown. The lit step's slice of the floor is the one mutable outline (path 0).
  - Stroke widths and colours come from the palette entries below, so a node's look is its shade.

The design and its contact sheet live in the operators repository (screens/theseus/concepts/d4_video.py, the
theseus-tower entry in library/, pattern P11).

    python3 gen_panel.py          # writes panel.svg.fragment, nodes.h and the display block in definition.json
Copyright (c) 2026 briannhinton. MIT."""
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = 'theseus'
G = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
CX = 240
FLOOR_Y = 194
GAP = 9
# the tower, bottom to top: (zone, radius, shell height, kind); the hat is drawn on the rod above the rack tom
TIERS = [(0.02, 86, 30, 'kick'), (0.25, 62, 22, 'floor'), (0.45, 50, 10, 'snare'), (0.65, 38, 15, 'tom'), (0.88, 36, 0, 'hat')]
RATES = [32, 24, 18, 14, 10, 8, 6, 4, 3, 2]
DEFAULT_RATE = 4                              # Rate 0.49 -> ten hits, as the engine reads it
SPARKS = 24
OVER_R = 62                                    # the shared overtone hoops are authored at the floor tom's size
SLICE_POINTS = 64

# palette: (name, fill, stroke, width)
PALETTE = [
    ('P_BLACK', '#000000', '#000000', 0.3),
    ('P_SHELL', '#000000', G[5], 1.2),          # a shell's silhouette
    ('P_SHELL_HOT', '#000000', G[7], 1.2),      # ... when its drum is hit
    ('P_HEAD', '#000000', G[6], 1.5),           # a head or cymbal at rest (and the snare's lighter shell)
    ('P_LIT1', G[1], G[7], 1.5), ('P_LIT2', G[2], G[7], 1.5), ('P_LIT3', G[3], G[7], 1.5),
    ('P_LIT4', G[4], G[7], 1.5), ('P_LIT5', G[5], G[7], 1.5),   # a struck head, filled by how hard and how open
    ('P_DIM', G[3], G[3], 0.3),                 # ribbons: hoops, lugs, wires, the rod
    ('P_G4', G[4], G[4], 0.3),                  # the floor clock's dashes
    ('P_G5', G[5], G[5], 0.3),                  # outer overtone hoops
    ('P_G6', G[6], G[6], 0.3),                  # rings, sparks, the bell, the snare's lugs
    ('P_HOT', G[7], G[7], 0.3),                 # the first ring, the lit dash
    ('P_MOUNT', '#000000', G[3], 1.0),          # the rack tom's mount
    ('P_SLICE', G[2], G[2], 0.6),               # the lit step's slice of the floor
]
P = {name: k for k, (name, *_rest) in enumerate(PALETTE)}


def f(v):
    s = f'{v:.1f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def ellipse(cx, cy, rx, ry, n=36, a0=0.0, a1=2 * math.pi):
    closed = a1 - a0 >= 2 * math.pi - 1e-6
    return [(cx + rx * math.cos(a0 + (a1 - a0) * k / n), cy + ry * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + (0 if closed else 1))]


def ribbon(pts, w):
    """an open line as a thin closed outline: one edge out, the other back w px away"""
    left, right = [], []
    for k, (x, y) in enumerate(pts):
        ax, ay = pts[max(0, k - 1)]
        bx, by = pts[min(len(pts) - 1, k + 1)]
        dx, dy = bx - ax, by - ay
        ln = math.hypot(dx, dy) or 1
        nx, ny = -dy / ln * w / 2, dx / ln * w / 2
        left.append((x + nx, y + ny))
        right.append((x - nx, y - ny))
    return left + right[::-1]


def hoop(cx, cy, rx, ry, w, n=24):
    """an elliptical ring as one outline: the outer round with a hairline gap at the back, then the inner round back"""
    gap = 0.8 / max(ry, 0.5)                    # the gap sits at the right-hand end, where the ring is w wide
    a0, a1 = gap / 2, 2 * math.pi - gap / 2
    ryw = w * ry / rx
    outer = ellipse(cx, cy, rx + w / 2, ry + ryw / 2, n, a0, a1)
    inner = ellipse(cx, cy, rx - w / 2, ry - ryw / 2, n, a0, a1)
    return outer + inner[::-1]


class Panel:
    def __init__(self):
        self.nodes = []            # dicts: pts (absolute), origin, shade, visible

    def add(self, pts, shade, origin=(0, 0), visible=True):
        assert 3 <= len(pts) <= 64, len(pts)
        self.nodes.append({'pts': pts, 'origin': origin, 'shade': shade, 'visible': visible})
        return len(self.nodes) - 1

    def svg(self, i):
        n = self.nodes[i]
        ox, oy = n['origin']
        name, fill, stroke, width = PALETTE[n['shade']]
        x, y = (ox, oy) if n['visible'] else (640, 320)
        d = 'M' + 'L'.join(f'{f(px - ox)} {f(py - oy)}' for px, py in n['pts']) + 'Z'     # compact: the fragment must stay under 64 KiB
        return f'<path id="{PREFIX}_node_{i}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'


def tops():
    """each tier's head centre at rest, bottom to top, and the hat's base"""
    y, out = FLOOR_Y, []
    for z, r, h, kind in TIERS:
        if kind == 'hat':
            out.append(y - 6)
            break
        out.append(y - h)
        y = y - h - GAP
    return out


def build():
    pn = Panel()
    order = []                                  # document order of node indices (the slice path is node 0)
    T = tops()
    idx = {}

    # node 0: the lit step's slice of the floor, a mutable outline (centre + 63 points round the step's arc)
    kr = TIERS[0][1]
    frx, fry = kr + 8, (kr + 8) * 0.26
    # authored at half size: a path's points must stay within 64 px of its origin, so display.cpp draws it at scale 2
    wedge = [(CX, FLOOR_Y + 1)] + ellipse(CX, FLOOR_Y + 1, (frx - 2) / 2, (fry - 1) / 2, SLICE_POINTS - 2, math.pi / 2, math.pi / 2 + 0.5)
    idx['slice'] = pn.add(wedge, P['P_SLICE'], (CX, FLOOR_Y + 1), visible=False)
    order.append(idx['slice'])
    # nodes 1..8: the header bar (written separately; reserve the indices)
    idx['hdr'] = len(pn.nodes)
    for i in range(8):
        pn.nodes.append(None)

    # the floor clock: one set of dashes per Rate setting
    idx['dash'] = []
    for ri, n in enumerate(RATES):
        idx['dash'].append(len(pn.nodes))
        gap = 0.18 if n <= 8 else 0.32
        seg = max(2, round(24 / n))
        for k in range(n):
            a0 = math.pi / 2 + 2 * math.pi * (k + gap / 2) / n
            a1 = math.pi / 2 + 2 * math.pi * (k + 1 - gap / 2) / n
            order.append(pn.add(ribbon(ellipse(CX, FLOOR_Y + 1, frx, fry, seg, a0, a1), 2.6), P['P_G4'], (CX, FLOOR_Y + 1), visible=ri == DEFAULT_RATE))

    # fm's overtone hoops: one shared set of four, thrown off whichever drum was hit last, behind the whole tower
    idx['over'] = len(pn.nodes)
    for k in range(4):
        order.append(pn.add(hoop(CX, 133, OVER_R, OVER_R * 0.26, 1.0), P['P_G6'] if k == 0 else P['P_G5'], (CX, 133), visible=False))

    # the drums
    tiers = []
    for t, (z, r, h, kind) in enumerate(TIERS[:-1]):
        top = T[t]
        ry = r * 0.26
        o = (CX, top)
        first = len(pn.nodes)
        rec = {}

        def add(pts, shade, visible=True):
            k = pn.add(pts, shade, o, visible)
            order.append(k)
            return k
        if kind == 'floor':
            for sx in (-1, 1):
                add(ribbon([(CX + sx * (r - 3), top + h * 0.35), (CX + sx * (r + 8), top + h + 6)], 1.4), P['P_DIM'])
        if kind == 'snare':
            for k in range(4):
                yy = top + h + ry * 0.55 + 1.6 * k
                add(ribbon([(CX - r * 0.6, yy), (CX + r * 0.6, yy)], 0.8), P['P_DIM'])
        body = [(CX - r, top)] + ellipse(CX, top + h, r, ry, 24, math.pi, 0) + [(CX + r, top)]
        rec['body_rest'] = P['P_HEAD'] if kind == 'snare' else P['P_SHELL']
        rec['body'] = add(body, rec['body_rest'])
        add(ribbon(ellipse(CX, top + h - 2, r, ry, 24, math.pi, 0), 1.0), P['P_DIM'])
        if kind == 'kick':
            for yy in (0.38, 0.6):
                add(ribbon(ellipse(CX, top + h * yy, r, ry, 24, math.pi, 0), 1.0), P['P_DIM'])
        if kind == 'tom':
            add([(CX - r - 7, top + h * 0.3), (CX - r, top + h * 0.3), (CX - r, top + h * 0.7), (CX - r - 7, top + h * 0.7)], P['P_MOUNT'])
        nl, inset, lug = {'kick': (8, 0.12, 'P_DIM'), 'floor': (4, 0.22, 'P_DIM'), 'snare': (8, 0.3, 'P_G6'), 'tom': (3, 0.22, 'P_DIM')}[kind]
        for k in range(nl):
            a = math.pi * (0.15 + 0.7 * k / max(1, nl - 1))
            lx, ly = CX + r * math.cos(a) * 0.98, top + h * 0.5 + ry * math.sin(a)
            add(ribbon([(lx, ly - h * inset), (lx, ly + h * inset)], 1.4), P[lug])
        rec['batter'] = add(ellipse(CX, top, r, ry, 32), P['P_HEAD'])
        rec['ring'] = len(pn.nodes)
        for k in range(4):                                   # decay's rings, spreading across the head
            add(hoop(CX, top, r * 0.84, ry * 0.84, 1.0, 20), P['P_HOT'] if k == 0 else P['P_G6'], visible=False)
        if kind == 'kick':
            add(hoop(CX + r * 0.3, top + ry * 0.12, r * 0.15, ry * 0.15, 1.0, 16), P['P_DIM'])
        rec.update({'first': first, 'count': len(pn.nodes) - first, 'top': top, 'r': r, 'ry': ry, 'zone': z})
        tiers.append(rec)

    # the hi-hat: a rod, two cymbals each pivoting on its own centre, and fm's shimmer arcs
    base = T[-1]
    r = TIERS[-1][1]
    ry = r * 0.22
    hat = {'base': base, 'r': r}
    hat['rod'] = pn.add(ribbon([(CX, base - 30), (CX, base + 6)], 1.4), P['P_DIM'], (CX, base))
    order.append(hat['rod'])
    for name, y in (('low', base + 2), ('top', base - 3)):
        hat[name] = len(pn.nodes)
        for pts, shade in ((ellipse(CX, y, r, ry, 32), 'P_HEAD'), (hoop(CX, y, r * 0.45, ry * 0.45, 0.8), 'P_DIM'),
                           (hoop(CX, y, r * 0.75, ry * 0.75, 0.8), 'P_DIM'), (hoop(CX, y - 0.6, r * 0.13, ry * 0.35, 1.0, 12), 'P_G6')):
            order.append(pn.add(pts, P[shade], (CX, y)))
    hat['shim'] = len(pn.nodes)
    for k in range(4):
        a = -0.5 - 0.45 * k
        order.append(pn.add(ribbon(ellipse(CX, base - 3, r + 4 + 3 * k, ry + 2 + k, 8, a - 0.3, a + 0.3), 1.0), P['P_G6'], (CX, base - 3), visible=False))

    # noise's sparks: one shared pool of small squares, authored round their own centres
    hat['spark'] = len(pn.nodes)
    for k in range(SPARKS):
        order.append(pn.add([(-1.5, -1.5), (1.5, -1.5), (1.5, 1.5), (-1.5, 1.5)], P['P_G6'], (0, 0), visible=False))
    return pn, order, idx, tiers, hat


def header_fragment(first):
    o = ['<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000" stroke="none"/>']
    for i in range(8):
        o.append(f'<path id="{PREFIX}_node_{first + i}" transform="matrix(1 0 0 1 {60 * i} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" fill="{G[i]}" stroke="{G[i]}" stroke-width="0.5"/>')
    for k in range(4):
        o.append(f'<text id="{PREFIX}_text_{k}" transform="matrix(1 0 0 1 {5 + 120 * k} 10)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for k in range(4):
        o.append(f'<text id="{PREFIX}_text_{4 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 17.4)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
    for k in range(4):
        o.append(f'<text id="{PREFIX}_text_{8 + k}" transform="matrix(1 0 0 1 {5 + 120 * k} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for k in range(4):
        o.append(f'<text id="{PREFIX}_text_{12 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')
    return o


def main():
    pn, order, idx, tiers, hat = build()
    total = len(pn.nodes)
    o = [f'<g id="{PREFIX}_panel">'] + header_fragment(idx['hdr'])
    o += [pn.svg(i) for i in order]
    for i, (name, fill, stroke, width) in enumerate(PALETTE):
        o.append(f'<path id="{PREFIX}_palette_{i}" transform="matrix(1 0 0 1 640 320)" d="M0,0 L4,0 L4,4 L0,4 Z" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    o.append('</g>')
    (HERE / 'panel.svg.fragment').write_text('\n'.join(o) + '\n', encoding='utf-8')

    tier_nodes = range(tiers[0]['first'], hat['spark'])
    q = lambda v: str(int(round(v * 1024)))
    h = ('#pragma once\n// theseus for the OP-XY: node indices and the tower\'s geometry, written by gen_panel.py.\n'
         '// Edit the generator, not this file.\nnamespace theseus {\n')
    h += f'enum {{ N_SLICE = {idx["slice"]}, N_HDR = {idx["hdr"]}, N_DASH = {idx["dash"][0]}, N_OVER = {idx["over"]}, OVER_R = {OVER_R}, N_TIERS = {tiers[0]["first"]}, N_TOTAL = {total} }};\n'
    h += 'enum { ' + ', '.join(f'{name} = {k}' for k, (name, *_r) in enumerate(PALETTE)) + f', PALETTE_N = {len(PALETTE)} }};\n'
    h += f'const int TOWER_X = {CX}, FLOOR_Y = {FLOOR_Y + 1};     // the floor clock\'s centre, px\n'
    h += f'const int FLOOR_RX = {q(frx_of())}, FLOOR_RY = {q(fry_of())};   // the slice\'s radii, Q10 px\n'
    h += f'const int RATE_N = {len(RATES)};\nconst unsigned char RATE_HITS[RATE_N] = {{ ' + ', '.join(map(str, RATES)) + ' };\n'
    h += 'const short DASH_FIRST[RATE_N] = { ' + ', '.join(map(str, idx['dash'])) + ' };\n'
    h += '// the drums, bottom to top: their nodes, rest heads, radii and the zone each one plays\n'
    h += f'enum {{ DRUM_N = {len(tiers)}, TIER_N = {len(TIERS)}, HAT = {len(tiers)} }};\n'
    for key, fmt in (('first', str), ('count', str), ('body', str), ('body_rest', str), ('batter', str), ('ring', str), ('top', str)):
        h += f'const short DRUM_{key.upper()}[DRUM_N] = {{ ' + ', '.join(fmt(t[key]) for t in tiers) + ' };\n'
    h += 'const short DRUM_R[DRUM_N] = { ' + ', '.join(str(t['r']) for t in tiers) + ' };              // px\n'
    h += 'const short DRUM_RY[DRUM_N] = { ' + ', '.join(q(t['ry']) for t in tiers) + ' };       // Q10 px\n'
    h += 'const short TIER_ZONE[TIER_N] = { ' + ', '.join(q(z) for z, *_r in TIERS) + ' };   // Q10\n'
    h += f'const int HAT_BASE = {hat["base"]}, HAT_R = {hat["r"]};\n'
    h += f'enum {{ HAT_ROD = {hat["rod"]}, HAT_LOW = {hat["low"]}, HAT_TOP = {hat["top"]}, HAT_PARTS = 4, HAT_SHIM = {hat["shim"]}, SPARK = {hat["spark"]}, SPARK_N = {SPARKS} }};\n'
    h += '// every node\'s origin y and rest shade, from N_TIERS up to the sparks (every origin sits on x = TOWER_X)\n'
    h += 'const short NODE_OY[SPARK - N_TIERS] = { ' + ', '.join(str(pn.nodes[i]['origin'][1]) for i in tier_nodes) + ' };\n'
    h += 'const unsigned char NODE_SHADE[SPARK - N_TIERS] = { ' + ', '.join(str(pn.nodes[i]['shade']) for i in tier_nodes) + ' };\n'
    h += '}\n'
    (HERE / 'nodes.h').write_text(h, encoding='utf-8')

    definition = json.loads((HERE / 'definition.json').read_text(encoding='utf-8'))
    d = definition['resources']['display']
    d.update({'stateBytes': STATE_BYTES, 'paths': 1, 'points': SLICE_POINTS, 'nodes': total, 'texts': 16, 'palette': len(PALETTE)})
    (HERE / 'definition.json').write_text(json.dumps(definition, indent=2) + '\n', encoding='utf-8')
    print(f'{total} nodes ({sum(t["count"] for t in tiers)} in the drums, {sum(RATES)} floor dashes, {SPARKS} sparks), {len(PALETTE)} palette entries')


def frx_of():
    return TIERS[0][1] + 8 - 2


def fry_of():
    return (TIERS[0][1] + 8) * 0.26 - 1


STATE_BYTES = 0                                  # sizeof(DisplayState) in display.h; set below


if __name__ == '__main__':
    STATE_BYTES = int((HERE / 'display.h').read_text(encoding='utf-8').split('// stateBytes ')[1].split()[0])
    main()
