#!/usr/bin/env python3
"""panel.svg.fragment and nodes.h for toro on the OP-XY: the RIDER screen as retained artwork.

The OP-XY draws the artwork itself; display.cpp only moves, turns, scales, repaints or hides each part every
frame. The device's drawing rules (Open-XY docs/display.md, "Drawing on the device") shape how the art is cut:

  - every path is ONE subpath of at most 64 points, so a figure is a GROUP of parts that share a pivot and get
    the same transform every frame (display.cpp moves a group with one call); nothing is a compound path
  - fills come from the artwork, so each part carries its own fill and stroke and is painted `keep`; the few
    parts that change grey (the eye highlights, the lasso) take a palette entry instead
  - lines are thin closed ribbons, tips have a small flat end, no turn comes within 2.5 degrees of straight back

Everything is authored in the OP-1's rig units (gen_svg.py / toro_screen.c): origin on the ram's top, the head
1.17x about (0, -30), the cowboy about his seat, his hat about the crown's base and his boots about the ankle.
display.cpp scales the lot by RIG. Limbs are authored pointing +x from their joint (the left side mirrored), so
a turn per frame bends them. Node indices are assigned here and written to nodes.h; keep display.cpp to those
names. The canvas is 480 x 220 with the two-page header on top; the art sits in 40..220.

    python3 gen_panel.py          # writes panel.svg.fragment, nodes.h and the node count into definition.json
Copyright (c) 2026 briannhinton. MIT."""
import json
import math
import re
from pathlib import Path

K = 0.5523
BLACK, EDGE, SEC, DEC, LITE, MID, DARK, WHITE = '#000000', '#CBCBCB', '#9A9A9A', '#5A5A5A', '#F0F0F0', '#707070', '#2A2A2A', '#FFFFFF'
BODY, PLATE, MUZ, CLOTH, HAT, HORN, TIP, LEATHER = '#121212', '#1E1E1E', '#050505', '#141414', '#101010', '#9A9A9A', '#D6D6D6', '#1A1A1A'
HK, HY = 1.17, -30            # the head's scale and lift
XS = 1.2                      # the bull is drawn a fifth wider than the OP-1's: the XY screen is wider than it is tall
CS, CY = 1.15, -52            # the cowboy's scale about his seat (the OP-1 drew him at 1.55; he is smaller here so the whole
                              # rig fits the XY's 180 px at a larger RIG: a small rider on a big bull)
HS, HATY = 1.45, -87          # his hat's scale about the crown's base
BS = 1.75                     # his boots' scale about the ankle
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


def poly(pts, close=True):
    return 'M' + ' L'.join(pt(p) for p in pts) + (' Z' if close else '')


def ribbon(pts, w=0.5):
    """a polyline as a thin closed ribbon offset along the normal, with square ends (no reversing turns)"""
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


def tube(spine, w0, w1, sign=1, n=14, scale=1.0, power=1.3):
    """a tapered tube along a cubic spine, flat-ended; sign mirrors in x"""
    pts = [(sign * x * scale, y * scale) for x, y in bezier(*spine, n)]
    up, dn = [], []
    for i, (x, y) in enumerate(pts):
        j = min(i, n - 1)
        dx, dy = pts[j + 1][0] - pts[j][0], pts[j + 1][1] - pts[j][1]
        ln = math.hypot(dx, dy) or 1
        w = (w0 + (w1 - w0) * (i / n) ** power) * scale / 2
        up.append((x - dy / ln * w, y + dx / ln * w)); dn.append((x + dy / ln * w, y - dx / ln * w))
    return poly(up + list(reversed(dn)))


def limb(length, w0, w1, sign=1):
    """a tapered limb from (0,0) along sign*x with round ends"""
    a = [(math.cos(math.pi / 2 + math.pi * i / 6) * w0 / 2 * sign, -math.sin(math.pi / 2 + math.pi * i / 6) * w0 / 2) for i in range(7)]
    b = [(length * sign + math.cos(-math.pi / 2 + math.pi * i / 6) * w1 / 2 * sign, math.sin(-math.pi / 2 + math.pi * i / 6) * w1 / 2) for i in range(7)]
    return poly(a + b)


def h(x, y):                                      # head-local -> rig units
    return (x * HK * XS, y * HK + HY)


def c(x, y):                                      # cowboy-local -> rig units relative to his seat
    return (x * CS, (y - CY) * CS)


def hs(x, y):                                     # hat-local -> rig units relative to the crown's base
    return (x * HS * CS, (y - HATY) * HS * CS)


def b(x, y, o):                                   # boot-local -> rig units relative to the ankle, toe toward o
    return (x * BS * CS * o, y * BS * CS)


class Panel:
    """collects parts into nodes and groups; writes the fragment and nodes.h"""
    def __init__(self):
        self.parts, self.groups, self.single = [], [], []
        self.static = []

    def group(self, name, parts):
        """parts: (d, fill, stroke, width) tuples sharing one pivot; returns the first node index"""
        first = len(self.parts)
        for p in parts:
            self.parts.append(p)
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
    S = P.static
    # ---- the header: the SDK's two-page bar steps are nodes 2..9, so paths 0/1 and the bar come first ----
    lasso = P.node('LASSO', poly([(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]), None, LITE, 1.5)
    rope = P.node('ROPE', poly([(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]), None, LITE, 1.5)
    assert lasso == 0 and rope == 1
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    P.group('HDR', [('M0,0 L60,0 L60,20 L0,20 Z', g, g, 0.5) for g in greys])

    # ---- static floor: the mat with its grooves, the hose with its clamps, the turntable with bolts ----
    cx0, base = 240, 208
    S.append(f'<path d="{ellipse(cx0, base + 6, 124, 9)}" fill="none" stroke="{DARK}" stroke-width="1.5"/>')
    S.append(f'<path d="{ellipse(cx0, base + 6, 96, 6.5)}" fill="none" stroke="{DARK}" stroke-width="1"/>')
    S.append(f'<path d="M{cx0 + 34},{base + 7} C300,{base + 15} 360,{base + 9} 400,{base - 1}" fill="none" stroke="{MID}" stroke-width="2"/>')
    for x in (300, 352):
        S.append(f'<path d="{ribbon([(x, base + 7), (x, base + 15)], 2.4)}" fill="{SEC}" stroke="{SEC}" stroke-width="0.5"/>')
    S.append(f'<path d="M400,{base - 1} L410,{base - 1}" fill="none" stroke="{DEC}" stroke-width="2"/>')
    S.append(f'<path d="{ellipse(410, base - 1, 3.5, 3.5)}" fill="{BLACK}" stroke="{SEC}" stroke-width="1.2"/>')
    # ---- shadow: a disc scaled per frame ----
    P.node('SHADOW', ellipse(0, 0, 30, 5), '#141414', '#141414', 0.5)
    # the turntable: base plate, a rim ring, four bolts, the cylinder with two pipe fittings
    S.append(f'<path d="{ellipse(cx0, base, 36, 6.5)}" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')
    S.append(f'<path d="{ellipse(cx0, base, 29, 4.8)}" fill="none" stroke="{DEC}" stroke-width="1"/>')
    for bx in (-24, -11, 11, 24):
        S.append(f'<path d="{ellipse(cx0 + bx, base + (0.9 if abs(bx) > 15 else 3.2), 1.8, 1.8)}" fill="{SEC}" stroke="{SEC}" stroke-width="0.5"/>')
    S.append(f'<path d="M{cx0 - 11},{base - 1} L{cx0 + 11},{base - 1} L{cx0 + 11},{base - 20} L{cx0 - 11},{base - 20} Z" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')
    for sd in (-1, 1):
        S.append(f'<path d="M{cx0 + sd * 11},{base - 9} L{cx0 + sd * 16},{base - 9} L{cx0 + sd * 16},{base - 14} L{cx0 + sd * 11},{base - 14} Z" fill="{BLACK}" stroke="{DEC}" stroke-width="1.2"/>')
    # ---- rod: a unit-height column mapped onto the ram, 16 px at scale 1, with a piston collar ----
    P.group('ROD', [('M-4.5,0 L4.5,0 L4.5,-16 L-4.5,-16 Z', '#9A9A9A', '#B8B8B8', 1),
                    ('M-6,-16 L6,-16 L6,-13 L-6,-13 Z', '#D6D6D6', '#D6D6D6', 0.5)])

    # ---- the bull's body (pivot the ram's top): barrel, seams, ears, handle; behind the cowboy ----
    def X(p): return (p[0] * XS, p[1])
    body = [('M' + pt(X((-36, -40))) + ' C' + ' '.join(pt(X(p)) for p in [(-26, -50), (26, -50), (36, -40)]) + ' C' + ' '.join(pt(X(p)) for p in [(47, -28), (47, -8), (34, -2)]) + ' L' + pt(X((-34, -2))) + ' C' + ' '.join(pt(X(p)) for p in [(-47, -8), (-47, -28), (-36, -40)]) + ' Z', BODY, SEC, 1.5)]
    for sd in (-1, 1):
        body.append((bez_ribbon(X((30 * sd, -44)), X((40 * sd, -36)), X((42 * sd, -18)), X((34 * sd, -4)), 1.0, 8), DEC, DEC, 0.5))
        body.append((poly([h(x * sd, y) for x, y in [(25, -12), (41, -11), (44, -6), (40, -2), (25, -2)]]), '#1B1B1B', SEC, 1.5))
        body.append((poly([h(x * sd, y) for x, y in [(29, -9.5), (39, -9), (40.5, -6), (37, -4), (29, -4)]]), BLACK, DEC, 0.8))   # the ear's inner slot
    body.append((bez_ribbon((-6, -52), (-6, -64), (6, -64), (6, -52), 1.8, 8), SEC, SEC, 0.5))
    for sd in (-1, 1):                                                  # the handle's mounts
        body.append((poly([(sd * 4, -50), (sd * 8.5, -50), (sd * 8.5, -54.5), (sd * 4, -54.5)]), '#1B1B1B', SEC, 1))
    P.group('BODY', body)

    # ---- the cowboy, seated behind the plate ----
    # thighs: from the hip along -x (left) / +x (right), 17.7 cowboy units
    P.node('THIGH_L', limb(17.7 * CS, 2.6 * CS, 2.0 * CS, -1), CLOTH, SEC, 1.5)
    P.node('THIGH_R', limb(17.7 * CS, 2.6 * CS, 2.0 * CS, 1), CLOTH, SEC, 1.5)
    # torso group (pivot the seat): the bottle shape, a vest with two front edges, the bandana with its knot, belt and buckle,
    # the rein arm gripping the handle
    torso = [(f'M{pt(c(-6, -54))} C{pt(c(-9.5, -73))} {pt(c(-6, -75))} {pt(c(0, -75))} C{pt(c(6, -75))} {pt(c(9.5, -73))} {pt(c(6, -54))} Z', CLOTH, SEC, 1.5)]
    for sd in (-1, 1):
        torso.append((bez_ribbon(c(sd * 4.2, -73), c(sd * 5.2, -66), c(sd * 4.6, -60), c(sd * 5.6, -55), 0.8, 8), DEC, DEC, 0.5))   # vest edges
    torso.append((poly([c(-5.6, -74.2), c(5.6, -74.2), c(0, -67.5)]), '#2E2E2E', EDGE, 1.2))          # bandana
    torso.append((ellipse(*c(0, -72.5), 1.1 * CS, 1.1 * CS), EDGE, EDGE, 0.5))                        # its knot
    torso.append((ribbon([c(-6.4, -57.5), c(6.4, -57.5)], 1.6), '#3A3A3A', '#3A3A3A', 0.5))            # belt
    torso.append((poly([c(-2.2, -59.3), c(2.2, -59.3), c(2.2, -55.7), c(-2.2, -55.7)]), '#202020', EDGE, 1.2))   # buckle
    torso.append((ribbon([c(-8.6, -71), c(-14, -63), c(-5, -61)], 1.9 * CS), CLOTH, SEC, 1.5))         # rein arm
    torso.append((ellipse(*c(-4.2, -61), 1.7 * CS, 1.7 * CS), CLOTH, SEC, 1.5))                        # its hand on the handle
    P.group('TORSO', torso)
    # head group (pivot the head centre): the head, brows, nose, cheek lines, ears
    hr = 6.6 * CS
    head = [(ellipse(0, 0, hr, hr), '#161616', SEC, 1.5)]
    for sd in (-1, 1):
        head.append((ribbon([(1.2 * sd * CS, -3.5 * CS), (3.9 * sd * CS, -2.6 * CS)], 1.0), LITE, LITE, 0.5))     # brows
        head.append((ribbon([(3.9 * sd * CS, 1.4 * CS), (3.2 * sd * CS, 3.4 * CS)], 0.7), DEC, DEC, 0.5))         # cheek lines
        head.append((ellipse(6.6 * sd * CS, 0.4 * CS, 1.0 * CS, 1.4 * CS), '#161616', SEC, 1.2))                  # ears
    head.append((ribbon([(0.3 * CS, -0.8 * CS), (1.0 * CS, 1.4 * CS), (-0.4 * CS, 1.6 * CS)], 0.7), SEC, SEC, 0.5))   # nose
    P.group('HEAD', head)
    # eyes (pivot the head centre, scaled wide): two rings and their pupils
    eyes = []
    for sd in (-1, 1):
        eyes.append((ellipse(2.6 * sd * CS, -1.1 * CS, 1.15 * CS, 1.15 * CS), BLACK, LITE, 1.2))
        eyes.append((poly([(2.8 * sd * CS - 0.5 * CS, -0.9 * CS - 0.5 * CS), (2.8 * sd * CS + 0.5 * CS, -0.9 * CS - 0.5 * CS), (2.8 * sd * CS + 0.5 * CS, -0.9 * CS + 0.5 * CS), (2.8 * sd * CS - 0.5 * CS, -0.9 * CS + 0.5 * CS)]), LITE, LITE, 0.5))   # pupils: squares, a tiny curve flattens to merged points
    P.group('EYES', eyes)
    P.node('SMILE', bez_ribbon((-2.7 * CS, 2.6 * CS), (-1.3 * CS, 4.3 * CS), (1.3 * CS, 4.3 * CS), (2.7 * CS, 2.6 * CS), 0.7, 8), LITE, LITE, 0.5)
    P.node('OMOUTH', ellipse(0, 3.5 * CS, 1.0 * CS, 1.3 * CS), BLACK, LITE, 1.2)
    # hat (pivot the crown's base): the brim lens, its underside shadow line, the crown, its crease, the band and its buckle
    hat = [(f'M{pt(hs(-16, -86.5))} C{pt(hs(-12, -83.6))} {pt(hs(12, -83.6))} {pt(hs(16, -86.5))} C{pt(hs(12, -86))} {pt(hs(-12, -86))} {pt(hs(-16, -86.5))} Z', HAT, LITE, 1.4)]
    P.group('BRIM', hat)
    crown = [(f'M{pt(hs(-7, -85.6))} C{pt(hs(-8, -92))} {pt(hs(-6.5, -96.5))} {pt(hs(-3, -96))} C{pt(hs(-1, -94.6))} {pt(hs(1, -94.6))} {pt(hs(3, -96))} '
              f'C{pt(hs(6.5, -96.5))} {pt(hs(8, -92))} {pt(hs(7, -85.6))} Z', HAT, LITE, 1.4),
             (bez_ribbon(hs(-2.2, -95.2), hs(-1.2, -92.5), hs(1.2, -92.5), hs(2.2, -95.2), 0.7, 6), DEC, DEC, 0.5),    # the crease
             (ribbon([hs(-7.3, -88.2), hs(7.3, -88.2)], 1.6), '#3A3A3A', '#3A3A3A', 0.5),                              # the band
             (poly([hs(-4.5, -89.2), hs(-2.5, -89.2), hs(-2.5, -87.2), hs(-4.5, -87.2)]), '#202020', EDGE, 1)]          # its buckle
    P.group('CROWN', crown)
    # lasso arm: upper arm from the shoulder, forearm with the hand ring from the elbow, both along +x
    P.node('UPPER', limb(9 * CS, 1.9 * CS, 1.7 * CS), CLOTH, SEC, 1.5)
    P.group('FORE', [(limb(10 * CS, 1.7 * CS, 1.6 * CS), CLOTH, SEC, 1.5), (ellipse(10 * CS, 0, 1.8 * CS, 1.8 * CS), CLOTH, SEC, 1.5)])

    # ---- the faceplate (pivot the ram's top): the twelve-sided plate, brow and cheek lines, rivets, muzzle and vents ----
    plate = [(poly([h(*p) for p in [(-20, -22), (20, -22), (27, -10), (24, 4), (17, 16), (14, 28), (8, 32), (-8, 32), (-14, 28), (-17, 16), (-24, 4), (-27, -10)]]), PLATE, EDGE, 2)]
    for sd in (-1, 1):
        plate.append((ribbon([h(27 * sd, -10), h(10 * sd, -7)], 1.0), DEC, DEC, 0.5))                 # brow lines
        plate.append((ribbon([h(24 * sd, 4), h(14 * sd, 13)], 0.8), DEC, DEC, 0.5))                   # cheek lines
        for rx, ry in ((23, -14), (22.5, 2), (15.5, 15)):                                            # rivets
            plate.append((ellipse(*h(rx * sd, ry), 1.6, 1.6), '#3A3A3A', SEC, 0.8))
    plate.append((ribbon([h(-19, -20), h(19, -20)], 1.0), DEC, DEC, 0.5))                             # the brow ridge
    plate.append((poly([h(*p) for p in [(-12, 13), (12, 13), (14, 24), (9, 31), (-9, 31), (-14, 24)]]), MUZ, DEC, 1.2))   # muzzle
    for sd in (-1, 1):
        plate.append((poly([h(x * sd, y) for x, y in [(3.5, 18), (9, 18), (8, 23), (4, 23)]]), BLACK, SEC, 0.8))   # vents
    P.group('PLATE', plate)
    # eye slits (pivot each slit's middle, scaled open): the OP-1's quads; highlights inside, lit on a note
    q = [(21, -9), (8, -5.2), (8, 1.5), (20, -1.8)]
    mx, my = sum(x for x, _ in q) / 4, sum(y for _, y in q) / 4
    P.node('EYE_L', poly([((x - mx) * -1 * HK, (y - my) * HK) for x, y in q]), BLACK, LITE, 1.2)
    P.node('EYE_R', poly([((x - mx) * HK, (y - my) * HK) for x, y in q]), BLACK, LITE, 1.2)
    hl = [(-4.5, -1.2), (4.5, -3.4), (4.5, -1.6), (-4.5, 0.4)]
    P.node('HILITE_L', poly([(x * -1 * HK, y * HK) for x, y in hl]), LITE, LITE, 0.3)
    P.node('HILITE_R', poly([(x * HK, y * HK) for x, y in hl]), LITE, LITE, 0.3)
    # shins (from the knee along -x/+x) and boots (hanging from the ankle, upright): shaft, heel block, toe cap, strap, spur bar
    P.node('SHIN_L', limb(15 * CS, 2.0 * CS, 1.8 * CS, -1), CLOTH, SEC, 1.5)
    P.node('SHIN_R', limb(15 * CS, 2.0 * CS, 1.8 * CS, 1), CLOTH, SEC, 1.5)
    for name, o in (('BOOT_L', -1), ('BOOT_R', 1)):
        shaft = (f'M{pt(b(-2.4, -1, o))} C{pt(b(-2.6, 6, o))} {pt(b(-2.6, 8, o))} {pt(b(1, 8.5, o))} L{pt(b(6.5, 8, o))} '
                 f'C{pt(b(7.5, 7.8, o))} {pt(b(7.5, 6, o))} {pt(b(5.5, 5.6, o))} L{pt(b(2.4, 4.5, o))} L{pt(b(2.4, -1, o))} Z')
        P.group(name, [(shaft, LEATHER, SEC, 1.5),
                       (poly([b(-2.6, 6.2, o), b(-0.2, 6.2, o), b(-0.2, 8.4, o), b(-2.6, 8.4, o)]), '#2A2A2A', SEC, 0.8),   # heel block
                       (bez_ribbon(b(4.2, 5.2, o), b(5.6, 5.4, o), b(6.6, 6.4, o), b(6.6, 7.6, o), 0.8, 6), DEC, DEC, 0.5),  # toe cap stitching
                       (ribbon([b(-2.5, 2.4, o), b(2.4, 2.4, o)], 0.9), DEC, DEC, 0.5),                                      # strap
                       (ribbon([b(-2.6, 6.6, o), b(-5.2, 6.6, o)], 0.9), SEC, SEC, 0.5)])                                   # spur bar
    # spur wheels (pivot the hub): a ring and four spokes, spun per frame
    r_sp = 2.8 * BS * CS
    for name in ('SPUR_L', 'SPUR_R'):
        P.group(name, [(ellipse(0, 0, 1.7 * BS * CS, 1.7 * BS * CS), BLACK, EDGE, 1.2)] +
                      [(ribbon([(math.cos(t) * r_sp, math.sin(t) * r_sp), (-math.cos(t) * r_sp, -math.sin(t) * r_sp)], 0.9), EDGE, EDGE, 0.5)
                       for t in (0, math.pi / 4, math.pi / 2, 3 * math.pi / 4)])

    # ---- horns: one curved tapered tube each (the OP-1's Bezier spine), a curling tip on its end, a collar at the root ----
    horn_spine = [(0, 0), (14, -3), (26, -7), (28, -17)]
    tip_spine = [(0, 0), (1, -4), (0, -8), (-2, -10)]
    P.node('HORN_L', tube(horn_spine, 5.6, 2.6, -1, 14, HK * 1.1), HORN, HORN, 1)
    P.node('HORN_R', tube(horn_spine, 5.6, 2.6, 1, 14, HK * 1.1), HORN, HORN, 1)
    P.node('TIP_L', tube(tip_spine, 2.6, 1.0, -1, 8, HK), TIP, TIP, 1)
    P.node('TIP_R', tube(tip_spine, 2.6, 1.0, 1, 8, HK), TIP, TIP, 1)
    P.node('COLLAR_L', ellipse(0, 0, 1.4 * HK, 3.8 * HK), '#3A3A3A', EDGE, 1.2)
    P.node('COLLAR_R', ellipse(0, 0, 1.4 * HK, 3.8 * HK), '#3A3A3A', EDGE, 1.2)
    # ---- nose ring: hangs from its pivot; a small bead at the bottom ----
    P.group('RING', [(ellipse(0, 7, 6.5, 6), None, LITE, 2), (ellipse(0, 13, 1.6, 1.6), LITE, LITE, 0.5)])
    # ---- steam puffs, sparks, the hose pulse ----
    P.group('STEAM', [('M-5,0 C-5,-7 5,-7 5,0 L4,0.4 C4,-5 -4,-5 -4,0.4 Z', None, SEC, 1.5)] * 4)
    P.group('SPARK', [(poly([(0, -3), (0.9, -0.9), (3, 0), (0.9, 0.9), (0, 3), (-0.9, 0.9), (-3, 0), (-0.9, -0.9)]), LITE, LITE, 0.5)] * 6)
    P.node('PULSE', ellipse(0, 0, 3, 3), LITE, LITE, 0.5)
    # ---- APHX tear slices and the lasso knot ----
    P.group('TEAR', [('M0,-0.6 L18,-0.6 L18,0.6 L0,0.6 Z', LITE, LITE, 0.5)] * 8)
    P.node('KNOT', ellipse(0, 0, 2.4, 2.4), BLACK, LITE, 1.5)

    # ---- write ----
    o = ['<g id="toro_panel">', '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    def element(i):
        d, fill, stroke, width = P.parts[i]
        x, y = (60 * (i - 2), 0) if 2 <= i < 10 else (640, 320)
        return f'<path id="toro_node_{i}" transform="matrix(1 0 0 1 {x} {y})" d="{d}" fill="{fill if fill else "none"}" stroke="{stroke}" stroke-width="{width}"/>'
    for i in range(len(P.parts)):
        if i >= 2:                                      # ids bind by name, document order is the stacking: the lasso's two outlines go on top
            o.append(element(i))
        if i == 9:                                      # the header's text slots go right after its bars, then the floor under everything else
            for k in range(4):
                o.append(f'<text id="toro_text_{k}" transform="matrix(1 0 0 1 {5 + 120 * k} 10)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{4 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 17.4)" fill="{"#CBCBCB" if k < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{8 + k}" transform="matrix(1 0 0 1 {5 + 120 * k} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
            for k in range(4):
                o.append(f'<text id="toro_text_{12 + k}" transform="matrix(1 0 0 1 {65 + 120 * k} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')
            o += S[:7]                                  # the mat, hose and clamps, under the shadow
        if i == 10:
            o += S[7:]                                  # the turntable, over the shadow
    o += [element(0), element(1)]
    outlines = ['#2A2A2A', '#3C3C3C', '#5A5A5A', '#707070', '#8C8C8C', '#A8A8A8', '#CBCBCB', '#F0F0F0']
    for i, s in enumerate(outlines):
        o.append(f'<circle id="toro_palette_{i}" cx="640" cy="0" r="1" fill="#101010" stroke="{s}" stroke-width="1.5"/>')
    solids = ['#1A1A1A', '#2A2A2A', '#4B4B4B', '#707070', '#9A9A9A', '#B8B8B8', '#D6D6D6', '#FFFFFF']
    for i, s in enumerate(solids):
        o.append(f'<circle id="toro_palette_{8 + i}" cx="640" cy="0" r="1" fill="{s}" stroke="{s}" stroke-width="1.5"/>')
    o.append('</g>')
    (HERE / 'panel.svg.fragment').write_text('\n'.join(o) + '\n', encoding='utf-8')
    (HERE / 'nodes.h').write_text(P.nodes_h(), encoding='utf-8')
    definition = json.loads((HERE / 'definition.json').read_text(encoding='utf-8'))
    definition['resources']['display']['nodes'] = len(P.parts)
    (HERE / 'definition.json').write_text(json.dumps(definition, indent=2) + '\n', encoding='utf-8')
    print(f'{len(P.parts)} nodes in {len(P.groups)} groups')


if __name__ == '__main__':
    main()
