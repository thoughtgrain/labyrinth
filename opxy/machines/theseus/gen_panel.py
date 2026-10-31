#!/usr/bin/env python3
"""panel.svg.fragment and nodes.h for theseus on the OP-XY: the PHALANX screen as retained artwork.

Static: the kit's shells, hoops, lugs and stands, the pedal, the drummer's cuirass. Moving parts are nodes that
display.cpp places, turns, scales, repaints or hides every frame: the five helmet heads (shown one at a time), the
crest's bristles, the eyes, the arms and sticks, the kick beater, the cymbals, ripple rings, sizzle specks and the
sway arcs. The device's drawing rules (Open-XY docs/display.md, "Drawing on the device") shape the cut: every path
is ONE subpath of at most 64 points, so a head is a GROUP of parts sharing the neck pivot, moved together by
display.cpp; fills come from the artwork, so parts carry their own greys and are painted `keep`; lines are thin
closed ribbons with flat ends and no turn near straight back.

The kit keeps the OP-1's layout in its units, stretched to the XY's wider screen: x by SX about the kit's centre,
y by SY from the header down. The helmet is uniform (HELM) about the neck pivot. Node indices are assigned here and
written to nodes.h.

    python3 gen_panel.py          # writes panel.svg.fragment, nodes.h and the node count into definition.json
Copyright (c) 2026 briannhinton. MIT."""
import json
import math
from pathlib import Path

K = 0.5523
SHELL, HEAD, EDGE, DEC, LITE, BLACK, SEC = '#141414', '#1E1E1E', '#B9B9C9', '#625D77', '#F0F0F0', '#000000', '#9A9A9A'
HELM, HELM_DARK, SLOT, BRONZE = '#4A4A4A', '#333333', '#060608', '#8C8C8C'
SX, SY = 1.6, 1.3             # the kit's stretch: OP-1 units -> pixels
HERE = Path(__file__).resolve().parent


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def pt(p):
    return f'{f(p[0])},{f(p[1])}'


def ellipse(cx, cy, rx, ry):
    return (f'M{f(cx + rx)},{f(cy)} C{f(cx + rx)},{f(cy + K * ry)} {f(cx + K * rx)},{f(cy + ry)} {f(cx)},{f(cy + ry)} '
            f'C{f(cx - K * rx)},{f(cy + ry)} {f(cx - rx)},{f(cy + K * ry)} {f(cx - rx)},{f(cy)} '
            f'C{f(cx - rx)},{f(cy - K * ry)} {f(cx - K * rx)},{f(cy - ry)} {f(cx)},{f(cy - ry)} '
            f'C{f(cx + K * rx)},{f(cy - ry)} {f(cx + rx)},{f(cy - K * ry)} {f(cx + rx)},{f(cy)} Z')


def shell(cx, cy, rx, ry, h):
    b = cy + h
    return (f'M{f(cx - rx)},{f(cy)} L{f(cx - rx)},{f(b)} C{f(cx - rx)},{f(b + K * ry)} {f(cx - K * rx)},{f(b + ry)} {f(cx)},{f(b + ry)} '
            f'C{f(cx + K * rx)},{f(b + ry)} {f(cx + rx)},{f(b + K * ry)} {f(cx + rx)},{f(b)} L{f(cx + rx)},{f(cy)} '
            f'C{f(cx + rx)},{f(cy + K * ry)} {f(cx + K * rx)},{f(cy + ry)} {f(cx)},{f(cy + ry)} '
            f'C{f(cx - K * rx)},{f(cy + ry)} {f(cx - rx)},{f(cy + K * ry)} {f(cx - rx)},{f(cy)} Z')


def poly(pts, close=True):
    return 'M' + ' L'.join(pt(p) for p in pts) + (' Z' if close else '')


def ribbon(pts, w=0.5):
    """a polyline as a thin closed ribbon offset along the normal, square ends"""
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


def bezier(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        u = i / n; v = 1 - u
        out.append((v ** 3 * p0[0] + 3 * v * v * u * p1[0] + 3 * v * u * u * p2[0] + u ** 3 * p3[0],
                    v ** 3 * p0[1] + 3 * v * v * u * p1[1] + 3 * v * u * u * p2[1] + u ** 3 * p3[1]))
    return out


def bez_ribbon(p0, p1, p2, p3, w, n=10):
    return ribbon(bezier(p0, p1, p2, p3, n), w)


def P(x, y):                                      # OP-1 kit units -> pixels
    return ((x - 160) * SX + 240, (y - 8) * SY + 40)


def static(d, fill, stroke, w):
    return f'<path d="{d}" fill="{fill if fill else "none"}" stroke="{stroke}" stroke-width="{w}"/>'


def line(a, b, col=DEC, w=1.5):
    return static(poly([a, b], False), None, col, w)


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
        lines = ['#pragma once', '// theseus for the OP-XY: node indices, written by gen_panel.py. Edit the generator, not this file.',
                 'namespace theseus {', 'enum {']
        for name, first, count in self.groups:
            lines.append(f'    N_{name} = {first}, N_{name}_COUNT = {count},')
        lines.append(f'    N_TOTAL = {len(self.parts)}')
        lines += ['};', '}', '']
        return '\n'.join(lines)


def main():
    Pn = Panel()
    ring64 = poly([(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)])
    Pn.node('RIPPLE', ring64, None, LITE, 1.5)
    Pn.node('SWAY', ring64, None, LITE, 1.5)
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    Pn.group('HDR', [('M0,0 L60,0 L60,20 L0,20 Z', g, g, 0.5) for g in greys])

    # ---- the static kit ----
    S = []
    # stands: hi-hat (94), ride (232), snare (122), floor tom legs, kick spurs; with tripod feet
    for a, b in (((94, 80), (94, 132)), ((232, 69), (234, 131)), ((122, 107), (122, 131)), ((197, 120), (195, 140)), ((217, 120), (219, 140)), ((143, 126), (136, 140)), ((177, 126), (184, 140))):
        S.append(line(P(*a), P(*b), DEC, 2))
    for c in ((87, 140), (94, 131), (101, 140)), ((227, 140), (234, 130), (241, 140)), ((115, 140), (122, 130), (129, 140)):
        S.append(static(poly([P(*p) for p in c], False), None, DEC, 1.5))
        for p in (c[0], c[2]):                                       # rubber feet
            x, y = P(*p); S.append(static(ellipse(x, y, 2.2, 1.4), DEC, DEC, 0.5))
    # the hi-hat's clutch and the ride stand's tilter
    x, y = P(94, 84); S.append(static(poly([(x - 2.5, y), (x + 2.5, y), (x + 2.5, y + 5), (x - 2.5, y + 5)]), SEC, SEC, 0.5))
    x, y = P(233, 72); S.append(static(ellipse(x, y, 2.4, 2.4), SEC, SEC, 0.5))
    # the drummer's cuirass: the breastplate, its central ridge, two shoulder plates and a skirt of pteruges strips
    cx, cy = P(158, 95)
    S.append(static(f'M{f(cx - 27)},{f(cy)} C{f(cx - 24)},{f(cy - 10)} {f(cx - 13)},{f(cy - 13)} {f(cx)},{f(cy - 13)} C{f(cx + 13)},{f(cy - 13)} {f(cx + 24)},{f(cy - 10)} {f(cx + 27)},{f(cy)} L{f(cx + 20)},{f(cy + 17)} L{f(cx - 20)},{f(cy + 17)} Z', SHELL, DEC, 1.5))
    S.append(static(ribbon([(cx, cy - 10), (cx, cy + 14)], 1.2), DEC, DEC, 0.5))
    for sd in (-1, 1):
        S.append(static(bez_ribbon((cx + sd * 10, cy - 11), (cx + sd * 16, cy - 2), (cx + sd * 14, cy + 6), (cx + sd * 18, cy + 15), 1.0, 8), DEC, DEC, 0.5))   # muscle lines
        S.append(static(poly([(cx + sd * 20, cy - 9), (cx + sd * 30, cy - 5), (cx + sd * 29, cy + 2), (cx + sd * 22, cy + 1)]), HELM_DARK, DEC, 1.2))    # shoulder plates
    for k in range(-3, 4):
        x0 = cx + k * 5.6
        S.append(static(poly([(x0 - 2.2, cy + 17), (x0 + 2.2, cy + 17), (x0 + 2.6, cy + 26), (x0 - 2.6, cy + 26)]), HELM_DARK, DEC, 1))                     # pteruges
    # drums: snare, floor tom, rack tom: shell, lugs, hoop ring, head
    for dcx, dcy, rx, ry, h, lugs in ((122, 98, 14, 4, 8, 5), (207, 108, 13, 4, 16, 5), (190, 88, 11, 3.5, 8, 4)):
        x, y = P(dcx, dcy)
        S.append(static(shell(x, y, rx * SX, ry * SY, h * SY), SHELL, EDGE, 1.5))
        for k in range(lugs):
            u = (k + 0.5) / lugs
            lx = x - rx * SX + 2 * rx * SX * u
            ly = y + math.sqrt(max(0, 1 - (2 * u - 1) ** 2)) * ry * SY + 3
            S.append(static(poly([(lx - 1.4, ly), (lx + 1.4, ly), (lx + 1.4, ly + 4.5), (lx - 1.4, ly + 4.5)]), SEC, SEC, 0.5))
        S.append(static(ellipse(x, y, rx * SX, ry * SY), HEAD, EDGE, 1.5))
        S.append(static(ellipse(x, y, rx * SX - 3, ry * SY - 1.2), None, DEC, 0.8))
    S.append(line(P(176, 99), P(186, 96), EDGE, 1.5))                                           # the rack tom's mount
    kx, ky = P(160, 114)
    S.append(static(ellipse(kx, ky, 22 * SX, 22 * SY), SHELL, EDGE, 2))
    S.append(static(ellipse(kx, ky, 18.5 * SX, 18.5 * SY), None, DEC, 1))
    for k in range(8):                                                                           # kick hoop claws
        a = k * math.pi / 4 + math.pi / 8
        lx, ly = kx + math.cos(a) * 20.2 * SX, ky + math.sin(a) * 20.2 * SY
        S.append(static(ellipse(lx, ly, 1.8, 1.8), SEC, SEC, 0.5))
    S.append(static(ellipse(kx, ky, 4.5, 4.5), None, DEC, 1))                                  # the port
    # the pedal: footboard, chain, and the beater's hinge block
    S.append(static(ribbon([P(158, 141.5), P(174, 139.5)], 2.6), LITE, LITE, 0.5))
    bx, by = P(166, 139)
    S.append(static(poly([(bx - 3, by - 2), (bx + 3, by - 2), (bx + 3, by + 2), (bx - 3, by + 2)]), SEC, SEC, 0.5))
    # the bottom hat, static (the top one is a node)
    hx, hy = P(94, 77)
    S.append(static(ellipse(hx, hy + 4, 13 * SX, 2.6 * SY), None, DEC, 1.5))

    # ---- the five heads: the OP-1's helmet geometry (helmet units, origin at the neck pivot, +x faces right), each a
    # group: bowl, stalk, holders, brow band, cheek rivets and the face openings. Left-facing heads mirror the right ----
    def bez(p1, p2, p3):
        return f'C{pt(p1)} {pt(p2)} {pt(p3)}'
    def M(p): return f'M{pt(p)}'
    def L(p): return f'L{pt(p)}'
    def profile(sign):
        m = lambda x, y: (x * sign, y)
        bowl = ' '.join([M(m(-16, 14)), bez(m(-20, 4), m(-21, -10), m(-14, -18)), bez(m(-8, -24), m(6, -25), m(13, -18)), bez(m(17, -14), m(19, -9), m(19.5, -4)),
                         L(m(21, 9)), L(m(17, 10.5)), L(m(14, 4.5)), L(m(12.5, 8)), bez(m(15, 12), m(18, 16), m(19, 20)), L(m(12, 22)), bez(m(6, 22), m(0, 18), m(-4, 16)),
                         bez(m(-8, 18), m(-12, 18), m(-16, 14)), 'Z'])
        stalk = ' '.join([M(m(12, -22)), bez(m(7, -38), m(-17, -39), m(-28, -13)), L(m(-26.5, -12.5)), bez(m(-16, -36), m(7, -35), m(10.5, -21.5)), 'Z'])
        return [(bowl, HELM, EDGE, 1.5), (stalk, BRONZE, BRONZE, 0.8),
                (poly([m(8, -20), m(12, -24), m(15, -19)]), BRONZE, EDGE, 1), (poly([m(-19, -13), m(-25, -14), m(-20, -9)]), BRONZE, EDGE, 1),
                (ribbon([m(-17, 2), m(-8, -2), m(4, -3), m(11, -4.4)], 1.2), HELM_DARK, HELM_DARK, 0.5),                   # brow band
                (poly([m(*p) for p in ((6.5, -7), (11, -10), (19.6, -9.6), (19.6, -5), (11, -4.4))]), SLOT, SLOT, 0.5),     # eye slot
                (poly([m(*p) for p in ((14.6, -5), (17, -5), (15.6, 4.8), (13.4, 4.8))]), SLOT, SLOT, 0.5),                # nose guard gap
                (ellipse(*m(-12, 10), 1.3, 1.3), BRONZE, BRONZE, 0.5), (ellipse(*m(-6, 14), 1.3, 1.3), BRONZE, BRONZE, 0.5),   # cheek rivets
                (ellipse(*m(15, 15), 1.3, 1.3), BRONZE, BRONZE, 0.5)]
    def three_quarter(sign):
        m = lambda x, y: (x * sign, y)
        bowl = ' '.join([M(m(-19, -3)), bez(m(-19.5, -17), m(-10, -24.5), m(1, -24)), bez(m(12, -23.5), m(18.5, -16), m(18.5, -3)), L(m(19, 7)),
                         bez(m(18.5, 15), m(15, 20), m(9, 22.5)), L(m(6, 22.5)), L(m(6, 9)), L(m(3, 9)), L(m(3, 23)), L(m(-3, 22)), bez(m(-10, 21), m(-16, 18), m(-18, 10)), 'Z'])
        stalk = ' '.join([M(m(12, -21)), bez(m(6, -35), m(-14, -36), m(-23, -14)), L(m(-21.5, -13.5)), bez(m(-13, -33), m(6, -32), m(10.5, -20.5)), 'Z'])
        return [(bowl, HELM, EDGE, 1.5), (stalk, BRONZE, BRONZE, 0.8),
                (poly([m(9, -19.5), m(12.5, -23), m(15, -18.5)]), BRONZE, EDGE, 1), (poly([m(-17, -12), m(-22, -14), m(-17.5, -9)]), BRONZE, EDGE, 1),
                (ribbon([m(-18, -4), m(-6, -5), m(8, -6), m(17.5, -4.5)], 1.2), HELM_DARK, HELM_DARK, 0.5),
                (poly([m(*p) for p in ((6, -9.6), (16.5, -9), (17.5, -4.5), (6, -4.2))]), SLOT, SLOT, 0.5),
                (poly([m(*p) for p in ((-9, -8.5), (3, -9.6), (3, -4.2), (-8.5, -4.2))]), SLOT, SLOT, 0.5),
                (poly([m(*p) for p in ((3, 9), (6, 9), (6, 22.5), (3, 23))]), SLOT, SLOT, 0.5),
                (ellipse(*m(-13, 12), 1.3, 1.3), BRONZE, BRONZE, 0.5), (ellipse(*m(14, 13), 1.3, 1.3), BRONZE, BRONZE, 0.5)]
    def front():
        bowl = ' '.join([M((-19.5, -4)), bez((-19.5, -17), (-11, -24), (0, -24)), bez((11, -24), (19.5, -17), (19.5, -4)), L((20, 6)), bez((19, 16), (14, 21), (7, 23)),
                         L((3, 23)), L((3, 9)), L((-3, 9)), L((-3, 23)), L((-7, 23)), bez((-14, 21), (-19, 16), (-20, 6)), 'Z'])
        stalk = ' '.join([M((-11, -20)), bez((-6, -29), (6, -29), (11, -20)), L((9.5, -19)), bez((5.5, -27), (-5.5, -27), (-9.5, -19)), 'Z'])
        return [(bowl, HELM, EDGE, 1.5), (stalk, BRONZE, BRONZE, 0.8),
                (poly([(-13, -18), (-10.5, -22), (-7.5, -18.5)]), BRONZE, EDGE, 1), (poly([(13, -18), (10.5, -22), (7.5, -18.5)]), BRONZE, EDGE, 1),
                (ribbon([(-19.5, -3.6), (19.5, -3.6)], 1.2), HELM_DARK, HELM_DARK, 0.5),
                (poly([(15.5, -9), (3, -10), (3, -4.5), (14.5, -4)]), SLOT, SLOT, 0.5), (poly([(-15.5, -9), (-3, -10), (-3, -4.5), (-14.5, -4)]), SLOT, SLOT, 0.5),
                (poly([(-3, 9), (3, 9), (3, 23), (-3, 23)]), SLOT, SLOT, 0.5),
                (ellipse(-14, 13, 1.3, 1.3), BRONZE, BRONZE, 0.5), (ellipse(14, 13, 1.3, 1.3), BRONZE, BRONZE, 0.5)]
    for name, parts in (('HEAD_0', profile(-1)), ('HEAD_1', three_quarter(-1)), ('HEAD_2', front()), ('HEAD_3', three_quarter(1)), ('HEAD_4', profile(1))):
        Pn.group(name, parts)
    # ---- crest bristles: a unit bristle pointing -y with a flat tip, placed along the stalk ----
    Pn.group('BRISTLE', [('M-0.9,0 L0.9,0 L0.35,-9 L-0.35,-9 Z', LITE, LITE, 0.4)] * 12)
    # ---- eyes: a disc each; rays: six spokes around the eye on strong hits ----
    Pn.node('EYE_L', ellipse(0, 0, 2.2, 2.2), LITE, LITE, 0.5)
    Pn.node('EYE_R', ellipse(0, 0, 2.2, 2.2), LITE, LITE, 0.5)
    Pn.group('RAYS', [(ribbon([(math.cos(a) * 3.5, math.sin(a) * 3.5), (math.cos(a) * 7.5, math.sin(a) * 7.5)], 1.2), LITE, LITE, 0.5)
                      for a in [k * math.pi / 3 + math.pi / 6 for k in range(6)]])
    # ---- arms: upper arm (pivot the shoulder, +x) with a bracer band; forearm + stick + tip (pivot the elbow) ----
    for name in ('UARM_L', 'UARM_R'):
        Pn.group(name, [(ribbon([(0, 0), (30, 0)], 5.5), HELM, EDGE, 1.5), (ribbon([(3, -3.2), (3, 3.2)], 2), BRONZE, BRONZE, 0.5)])
    for name in ('FARM_L', 'FARM_R'):
        Pn.group(name, [(ribbon([(0, 0), (26, 0)], 4.5), HELM, EDGE, 1.5), (ribbon([(20, -2.6), (20, 2.6)], 2), BRONZE, BRONZE, 0.5),   # bracer
                        (ribbon([(22, 0), (58, 0)], 2.4), '#D6D6D6', EDGE, 1), (ellipse(58.8, 0, 1.9, 1.9), LITE, LITE, 0.5)])            # stick and tip
    # ---- kick beater (pivot the pedal, pointing -y): shaft and felt head; the thump diamond ----
    Pn.group('BEATER', [(ribbon([(0, 0), (0, -24)], 2), LITE, LITE, 1), (ellipse(0, -25.5, 3.6, 3.6), '#2A2A2A', LITE, 1.2)])
    Pn.node('THUMP', 'M-7,0 L0,-7 L7,0 L0,7 Z', None, LITE, 1)
    # ---- hi-hat top and ride cymbal: a plate, a bell, and a lathe ring; wobble per frame ----
    def cymbal(rx, ry):
        return [(ellipse(0, 0, rx, ry), '#101010', EDGE, 1.5), (ellipse(0, -0.6, rx * 0.55, ry * 0.55), None, DEC, 0.8),
                ('M-5,-1 C-5,-5 5,-5 5,-1 L4,-0.6 C4,-4 -4,-4 -4,-0.6 Z', None, EDGE, 1.5)]
    Pn.group('HAT', cymbal(13 * SX, 3 * SY))
    Pn.group('CYM', cymbal(17 * SX, 3 * SY))
    # ---- ripple rings (scaled per frame), sizzle specks, sway arcs ----
    Pn.group('RING', [(ellipse(0, 0, 16, 16), None, LITE, 1)] * 8)
    Pn.group('SPECK', [(poly([(0, -1.6), (1.6, 0), (0, 1.6), (-1.6, 0)]), LITE, LITE, 0.5)] * 8)
    Pn.group('ARC', [(ribbon([(math.cos(a) * 30, math.sin(a) * 30) for a in [-0.5 + k * 0.1 for k in range(11)]], 1.2), None, LITE, 1)] * 2)

    # ---- write ----
    o = ['<g id="theseus_panel">', '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    def element(i):
        d, fill, stroke, width = Pn.parts[i]
        x, y = (60 * (i - 2), 0) if 2 <= i < 10 else (640, 320)
        return f'<path id="theseus_node_{i}" transform="matrix(1 0 0 1 {x} {y})" d="{d}" fill="{fill if fill else "none"}" stroke="{stroke}" stroke-width="{width}"/>'
    for i in range(len(Pn.parts)):
        if i >= 2:
            o.append(element(i))
        if i == 9:
            for k in range(4):
                o.append(f'<text id="theseus_text_{k}" transform="matrix(1 0 0 1 {5 + 120 * k} 10)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="theseus_text_{4 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 17.4)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
            for k in range(4):
                o.append(f'<text id="theseus_text_{8 + k}" transform="matrix(1 0 0 1 {5 + 120 * k} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="theseus_text_{12 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')
            o += S                                      # the kit, under every moving part
    o += [element(0), element(1)]                       # the two outlines on top
    outlines = ['#2A2A2A', '#3C3C3C', '#5A5A5A', '#707070', '#8C8C8C', '#A8A8A8', '#CBCBCB', '#F0F0F0']
    for i, s in enumerate(outlines):
        o.append(f'<circle id="theseus_palette_{i}" cx="640" cy="0" r="1" fill="#1C1C1C" stroke="{s}" stroke-width="1.5"/>')
    solids = ['#060608', '#2A2A2A', '#4B4B4B', '#707070', '#9A9A9A', '#B8B8B8', '#D6D6D6', '#FFFFFF']
    for i, s in enumerate(solids):
        o.append(f'<circle id="theseus_palette_{8 + i}" cx="640" cy="0" r="1" fill="{s}" stroke="{s}" stroke-width="1.5"/>')
    o.append('</g>')
    (HERE / 'panel.svg.fragment').write_text('\n'.join(o) + '\n', encoding='utf-8')
    (HERE / 'nodes.h').write_text(Pn.nodes_h(), encoding='utf-8')
    definition = json.loads((HERE / 'definition.json').read_text(encoding='utf-8'))
    definition['resources']['display']['nodes'] = len(Pn.parts)
    (HERE / 'definition.json').write_text(json.dumps(definition, indent=2) + '\n', encoding='utf-8')
    print(f'{len(Pn.parts)} nodes in {len(Pn.groups)} groups')


if __name__ == '__main__':
    main()
