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


def hatch(pts_a, pts_b, n, w=0.9):
    """n short ink strokes between two polylines of equal length: shading the way a print hatches, each its own part"""
    out = []
    for k in range(n):
        u = (k + 0.5) / n
        i = min(int(u * (len(pts_a) - 1)), len(pts_a) - 2)
        t = u * (len(pts_a) - 1) - i
        ax, ay = pts_a[i][0] + (pts_a[i + 1][0] - pts_a[i][0]) * t, pts_a[i][1] + (pts_a[i + 1][1] - pts_a[i][1]) * t
        bx, by = pts_b[i][0] + (pts_b[i + 1][0] - pts_b[i][0]) * t, pts_b[i][1] + (pts_b[i + 1][1] - pts_b[i][1]) * t
        out.append((ribbon([(ax, ay), (bx, by)], w), INK, INK, 0.3))
    return out


def star(cx, cy, r, points=4, inner=0.28):
    pts = []
    for k in range(points * 2):
        a = -math.pi / 2 + k * math.pi / points
        rr = r if k % 2 == 0 else r * inner
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    return poly(pts)


def taper(pts, w0, w1):
    """a brush stroke: a polyline as a ribbon whose width runs w0 -> w1"""
    n = len(pts)
    nx, ny = [], []
    for i in range(n - 1):
        dx, dy = pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]
        ln = math.hypot(dx, dy) or 1
        nx.append(-dy / ln); ny.append(dx / ln)
    nx.append(nx[-1]); ny.append(ny[-1])
    up, dn = [], []
    for i, (x, y) in enumerate(pts):
        w = (w0 + (w1 - w0) * i / (n - 1)) / 2
        up.append((x + nx[i] * w, y + ny[i] * w)); dn.append((x - nx[i] * w, y - ny[i] * w))
    return poly(up + list(reversed(dn)))


INK, PAPER, CREAM, CLOTH_D, CLOTH_L, WHITE_ = '#0A0A0A', '#C9C9C9', '#E2E2E2', '#6E6E6E', '#A4A4A4', '#FFFFFF'


def main():
    P = Panel()
    S = P.static
    # ---- paths 0/1 (the lasso loop and the rope) and the header's bar steps come first ----
    lasso = P.node('LASSO', poly([(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]), None, CREAM, 2)
    rope = P.node('ROPE', poly([(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]), None, CREAM, 1.5)
    assert lasso == 0 and rope == 1
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    P.group('HDR', [('M0,0 L60,0 L60,20 L0,20 Z', g, g, 0.5) for g in greys])

    # ---- static: ornaments in the corners (the print's starbursts and crosses), the mat, the hose, the turntable ----
    cx0, base = 240, 208
    for sx, sy in ((40, 70), (440, 70)):
        S.append(f'<path d="{star(sx, sy, 9, 4, 0.3)}" fill="{CREAM}" stroke="{CREAM}" stroke-width="0.5"/>')
        S.append(f'<path d="{star(sx + 16, sy + 14, 4, 4, 0.3)}" fill="{PAPER}" stroke="{PAPER}" stroke-width="0.5"/>')
        S.append(f'<path d="{star(sx - 14, sy + 16, 3, 4, 0.3)}" fill="{PAPER}" stroke="{PAPER}" stroke-width="0.5"/>')
    for sx, sy in ((28, 150), (452, 150)):
        S.append(f'<path d="{ribbon([(sx - 6, sy), (sx + 6, sy)], 1.4)}" fill="{CLOTH_L}" stroke="{CLOTH_L}" stroke-width="0.3"/>')
        S.append(f'<path d="{ribbon([(sx, sy - 6), (sx, sy + 6)], 1.4)}" fill="{CLOTH_L}" stroke="{CLOTH_L}" stroke-width="0.3"/>')
    # the mat: a brushy ground line with a few dust strokes, like the print's shadow under the horse
    S.append(f'<path d="{ellipse(cx0, base + 6, 126, 9)}" fill="none" stroke="{CLOTH_D}" stroke-width="2"/>')
    for k in range(6):
        x = cx0 - 100 + k * 40
        S.append(f'<path d="{taper([(x, base + 1), (x + 14, base + 1.5)], 2.2, 0.6)}" fill="{CLOTH_L}" stroke="{CLOTH_L}" stroke-width="0.3"/>')
    # the hose: a rope-textured line to the FOLD readout, with clamps
    S.append(f'<path d="M{cx0 + 34},{base + 7} C300,{base + 15} 360,{base + 9} 400,{base - 1}" fill="none" stroke="{PAPER}" stroke-width="2.4"/>')
    for k in range(9):
        u = (k + 0.5) / 9
        bx = (1 - u) ** 3 * (cx0 + 34) + 3 * (1 - u) ** 2 * u * 300 + 3 * (1 - u) * u * u * 360 + u ** 3 * 400
        by = (1 - u) ** 3 * (base + 7) + 3 * (1 - u) ** 2 * u * (base + 15) + 3 * (1 - u) * u * u * (base + 9) + u ** 3 * (base - 1)
        S.append(f'<path d="{ribbon([(bx - 1, by - 2.2), (bx + 1, by + 2.2)], 0.9)}" fill="{INK}" stroke="{INK}" stroke-width="0.3"/>')
    S.append(f'<path d="M400,{base - 1} L410,{base - 1}" fill="none" stroke="{CLOTH_L}" stroke-width="2"/>')
    S.append(f'<path d="{ellipse(410, base - 1, 3.5, 3.5)}" fill="{INK}" stroke="{CREAM}" stroke-width="1.5"/>')
    # ---- shadow ----
    P.node('SHADOW', ellipse(0, 0, 30, 5), '#1A1A1A', '#1A1A1A', 0.5)
    # the turntable, print style: a light disc with an ink rim and bolts, the cylinder with hatching on its shaded side
    S.append(f'<path d="{ellipse(cx0, base, 36, 6.5)}" fill="{PAPER}" stroke="{INK}" stroke-width="1"/>')
    S.append(f'<path d="{ellipse(cx0, base, 29, 4.8)}" fill="none" stroke="{INK}" stroke-width="1.2"/>')
    for bx in (-24, -11, 11, 24):
        S.append(f'<path d="{ellipse(cx0 + bx, base + (0.9 if abs(bx) > 15 else 3.2), 1.8, 1.8)}" fill="{INK}" stroke="{INK}" stroke-width="0.5"/>')
    S.append(f'<path d="M{cx0 - 11},{base - 1} L{cx0 + 11},{base - 1} L{cx0 + 11},{base - 20} L{cx0 - 11},{base - 20} Z" fill="{PAPER}" stroke="{PAPER}" stroke-width="0.5"/>')
    for k in range(4):
        y = base - 18 + k * 4.2
        S.append(f'<path d="{ribbon([(cx0 + 3, y), (cx0 + 10, y + 1.2)], 1.0)}" fill="{INK}" stroke="{INK}" stroke-width="0.3"/>')
    for sd in (-1, 1):
        S.append(f'<path d="M{cx0 + sd * 11},{base - 9} L{cx0 + sd * 16},{base - 9} L{cx0 + sd * 16},{base - 14} L{cx0 + sd * 11},{base - 14} Z" fill="{INK}" stroke="{PAPER}" stroke-width="1.2"/>')
    # ---- rod: the ram, light with an ink highlight line, and a collar ----
    P.group('ROD', [('M-4.5,0 L4.5,0 L4.5,-16 L-4.5,-16 Z', PAPER, PAPER, 0.5), (ribbon([(-2.2, -1), (-2.2, -15)], 1.0), INK, INK, 0.3),
                    ('M-6,-16 L6,-16 L6,-13 L-6,-13 Z', CREAM, INK, 0.8)])

    # ---- the bull's body (pivot the ram's top): a light barrel with ink seams, hatching under the belly, ears, handle ----
    def X(p): return (p[0] * XS, p[1])
    barrel_pts = bezier(X((-36, -40)), X((-26, -50)), X((26, -50)), X((36, -40)), 10) + bezier(X((36, -40)), X((47, -28)), X((47, -8)), X((34, -2)), 8)[1:] + \
        [X((-34, -2))] + bezier(X((-34, -2)), X((-47, -8)), X((-47, -28)), X((-36, -40)), 8)[1:-1]
    body = [(poly(barrel_pts), PAPER, INK, 2.2)]
    for sd in (-1, 1):
        body.append((taper(bezier(X((30 * sd, -44)), X((40 * sd, -36)), X((42 * sd, -18)), X((34 * sd, -4)), 8), 2.4, 0.8), INK, INK, 0.3))   # flank seams
    # belly hatching: short strokes along the lower edge, both sides
    for sd in (-1, 1):
        lo = [X((sd * (34 - k * 4), -2 - k * 0.3)) for k in range(6)]
        hi = [X((sd * (33 - k * 4), -9 - k * 0.8)) for k in range(6)]
        body += hatch(lo, hi, 5, 1.0)
    for sd in (-1, 1):
        ear = [h(x * sd, y) for x, y in [(25, -12), (41, -11), (44, -6), (40, -2), (25, -2)]]
        body.append((poly(ear), PAPER, INK, 1.8))
        body.append((poly([h(x * sd, y) for x, y in [(29, -9.5), (39, -9), (40.5, -6), (37, -4), (29, -4)]]), INK, INK, 0.5))
    body.append((taper(bezier((-6, -52), (-6, -64), (6, -64), (6, -52), 8), 2.4, 2.4), CREAM, CREAM, 0.3))
    for sd in (-1, 1):
        body.append((poly([(sd * 4, -50), (sd * 8.5, -50), (sd * 8.5, -54.5), (sd * 4, -54.5)]), INK, CREAM, 0.8))
    P.group('BODY', body)

    # ---- the cowboy ----
    P.node('THIGH_L', limb(17.7 * CS, 2.8 * CS, 2.2 * CS, -1), CLOTH_L, INK, 0.8)
    P.node('THIGH_R', limb(17.7 * CS, 2.8 * CS, 2.2 * CS, 1), CLOTH_L, INK, 0.8)
    # torso: a light shirt, a dark vest in two panels with ink edges, a bandana with print dots, belt and buckle, the rein arm
    torso = [(poly(bezier(c(-6, -54), c(-9.5, -73), c(-6, -75), c(0, -75), 6) + bezier(c(0, -75), c(6, -75), c(9.5, -73), c(6, -54), 6)[1:]), CREAM, INK, 1.4)]
    for sd in (-1, 1):
        vest = bezier(c(sd * 6.2, -73.5), c(sd * 7.6, -66), c(sd * 7, -60), c(sd * 6.2, -55), 6) + bezier(c(sd * 6.2, -55), c(sd * 5, -58), c(sd * 3.4, -64), c(sd * 3.2, -72.5), 6)[1:]
        torso.append((poly(vest), CLOTH_D, INK, 0.8))
    torso.append((poly([c(-6.2, -75.2), c(6.2, -75.2), c(0, -66.5)]), INK, CREAM, 0.8))                   # bandana
    for dx, dy in ((-2.6, -72.6), (0, -71), (2.6, -72.6), (-1.3, -69.4), (1.3, -69.4)):
        px, py = c(dx, dy); r = 0.7 * CS
        torso.append((poly([(px, py - r), (px + r, py), (px, py + r), (px - r, py)]), CREAM, CREAM, 0.3))      # its print dots (diamonds: tiny curves merge on the device)
    torso.append((ellipse(*c(0, -74.4), 1.3 * CS, 1.1 * CS), CREAM, INK, 0.8))                              # the knot
    torso.append((ribbon([c(-6.4, -57.5), c(6.4, -57.5)], 1.8), INK, INK, 0.3))                             # belt
    torso.append((poly([c(-2.4, -59.5), c(2.4, -59.5), c(2.4, -55.5), c(-2.4, -55.5)]), CREAM, INK, 0.8))   # buckle
    torso.append((taper([c(-8.6, -71), c(-14, -63), c(-5, -61)], 2.2 * CS, 1.7 * CS), CREAM, INK, 0.8))     # rein arm
    torso.append((ellipse(*c(-4.2, -61), 1.8 * CS, 1.8 * CS), CREAM, INK, 0.8))                             # hand
    P.group('TORSO', torso)
    # head: a light face with ink brows, nose, moustache, ears and a jaw line
    hr = 6.8 * CS
    head = [(ellipse(0, 0, hr, hr), CREAM, INK, 1.4)]
    for sd in (-1, 1):
        head.append((taper([(1.2 * sd * CS, -3.6 * CS), (3.9 * sd * CS, -2.9 * CS)], 1.4, 0.6), INK, INK, 0.3))     # brows
        head.append((ellipse(6.6 * sd * CS, 0.4 * CS, 1.1 * CS, 1.5 * CS), CREAM, INK, 0.8))                        # ears
        head.append((taper(bezier((0.3 * sd * CS, 2.3 * CS), (1.8 * sd * CS, 1.6 * CS), (3.4 * sd * CS, 2.0 * CS), (4.2 * sd * CS, 3.4 * CS), 6), 1.9, 0.5), INK, INK, 0.3))   # moustache
    head.append((taper([(0.2 * CS, -0.9 * CS), (1.1 * CS, 1.3 * CS), (-0.3 * CS, 1.6 * CS)], 0.9, 0.9), INK, INK, 0.3))                           # nose
    head.append((taper(bezier((-3.5 * CS, 4.2 * CS), (-1.5 * CS, 6.3 * CS), (1.5 * CS, 6.3 * CS), (3.5 * CS, 4.2 * CS), 6), 1.2, 1.2), INK, INK, 0.3))   # jaw
    P.group('HEAD', head)
    eyes = []
    for sd in (-1, 1):
        eyes.append((ellipse(2.6 * sd * CS, -1.1 * CS, 1.25 * CS, 1.25 * CS), WHITE_, INK, 0.8))
        eyes.append((poly([(2.8 * sd * CS - 0.55 * CS, -0.9 * CS - 0.55 * CS), (2.8 * sd * CS + 0.55 * CS, -0.9 * CS - 0.55 * CS), (2.8 * sd * CS + 0.55 * CS, -0.9 * CS + 0.55 * CS), (2.8 * sd * CS - 0.55 * CS, -0.9 * CS + 0.55 * CS)]), INK, INK, 0.3))
    P.group('EYES', eyes)
    P.node('SMILE', taper(bezier((-2.7 * CS, 2.9 * CS), (-1.3 * CS, 4.4 * CS), (1.3 * CS, 4.4 * CS), (2.7 * CS, 2.9 * CS), 8), 1.0, 1.0), INK, INK, 0.3)
    P.node('OMOUTH', ellipse(0, 3.6 * CS, 1.1 * CS, 1.4 * CS), INK, INK, 0.5)
    # hat: a light brim with hatching on its underside, a dark band with a buckle, a light crown with a crease; ink contours
    brim = [(poly(bezier(hs(-16, -86.5), hs(-12, -83.6), hs(12, -83.6), hs(16, -86.5), 10) + bezier(hs(16, -86.5), hs(12, -86), hs(-12, -86), hs(-16, -86.5), 10)[1:-1]), CREAM, INK, 1.4)]
    lo = bezier(hs(-14, -86.3), hs(-10, -84.2), hs(10, -84.2), hs(14, -86.3), 8); hi = bezier(hs(-14, -86.2), hs(-10, -85.6), hs(10, -85.6), hs(14, -86.2), 8)
    brim += hatch(lo, hi, 7, 0.8)
    P.group('BRIM', brim)
    crown = [(poly(bezier(hs(-7, -85.6), hs(-8, -92), hs(-6.5, -96.5), hs(-3, -96), 6) + bezier(hs(-3, -96), hs(-1, -94.6), hs(1, -94.6), hs(3, -96), 4)[1:] + bezier(hs(3, -96), hs(6.5, -96.5), hs(8, -92), hs(7, -85.6), 6)[1:]), CREAM, INK, 1.4),
             (taper(bezier(hs(-2.2, -95.2), hs(-1.2, -92.5), hs(1.2, -92.5), hs(2.2, -95.2), 6), 1.0, 1.0), INK, INK, 0.3),
             (ribbon([hs(-7.3, -88.2), hs(7.3, -88.2)], 2.0), INK, INK, 0.3),
             (poly([hs(-4.6, -89.3), hs(-2.4, -89.3), hs(-2.4, -87.1), hs(-4.6, -87.1)]), CREAM, INK, 0.6)]
    # crown hatching on the shaded right side
    crown += hatch([hs(4.2, -87.5), hs(5.5, -90.5), hs(6.2, -93.5)], [hs(6.4, -87.3), hs(7.2, -90.2), hs(7.0, -93.2)], 3, 0.8)
    P.group('CROWN', crown)
    P.node('UPPER', limb(9 * CS, 2.1 * CS, 1.9 * CS), CREAM, INK, 0.8)
    P.group('FORE', [(limb(10 * CS, 1.9 * CS, 1.7 * CS), CREAM, INK, 0.8), (ellipse(10 * CS, 0, 1.9 * CS, 1.9 * CS), CREAM, INK, 0.8)])

    # ---- the faceplate: a light plate with ink contours, brow ridge, cheek lines, rivets, a dark muzzle with light nostrils ----
    plate_pts = [h(*p) for p in [(-20, -22), (20, -22), (27, -10), (24, 4), (17, 16), (14, 28), (8, 32), (-8, 32), (-14, 28), (-17, 16), (-24, 4), (-27, -10)]]
    plate = [(poly(plate_pts), PAPER, INK, 2.6)]
    for sd in (-1, 1):
        plate.append((taper([h(27 * sd, -10), h(10 * sd, -7)], 2.0, 0.8), INK, INK, 0.3))
        plate.append((taper([h(24 * sd, 4), h(14 * sd, 13)], 1.6, 0.6), INK, INK, 0.3))
        for rx, ry in ((23, -14), (22.5, 2), (15.5, 15)):
            plate.append((ellipse(*h(rx * sd, ry), 1.7, 1.7), INK, INK, 0.5))
        # cheek hatching
        plate += hatch([h(sd * 25, -6), h(sd * 23.5, 0), h(sd * 20, 7)], [h(sd * 20, -5), h(sd * 19, 0.5), h(sd * 16.5, 6.5)], 4, 0.8)
    plate.append((ribbon([h(-19, -20), h(19, -20)], 1.4), INK, INK, 0.3))
    plate.append((poly([h(*p) for p in [(-12, 13), (12, 13), (14, 24), (9, 31), (-9, 31), (-14, 24)]]), INK, INK, 0.8))
    for sd in (-1, 1):
        plate.append((poly([h(x * sd, y) for x, y in [(3.5, 18), (9, 18), (8, 23), (4, 23)]]), CREAM, CREAM, 0.5))
    P.group('PLATE', plate)
    q = [(21, -9), (8, -5.2), (8, 1.5), (20, -1.8)]
    mx, my = sum(x for x, _ in q) / 4, sum(y for _, y in q) / 4
    P.node('EYE_L', poly([((x - mx) * -1 * HK, (y - my) * HK) for x, y in q]), INK, INK, 1.2)
    P.node('EYE_R', poly([((x - mx) * HK, (y - my) * HK) for x, y in q]), INK, INK, 1.2)
    hl = [(-4.5, -1.2), (4.5, -3.4), (4.5, -1.6), (-4.5, 0.4)]
    P.node('HILITE_L', poly([(x * -1 * HK, y * HK) for x, y in hl]), CREAM, CREAM, 0.3)
    P.node('HILITE_R', poly([(x * HK, y * HK) for x, y in hl]), CREAM, CREAM, 0.3)
    # shins with chaps fringe, boots dark with light stitching, spurs
    for name, sd in (('SHIN_L', -1), ('SHIN_R', 1)):
        P.node(name, limb(15 * CS, 2.2 * CS, 2.0 * CS, sd), CLOTH_L, INK, 0.8)
    for name, o in (('BOOT_L', -1), ('BOOT_R', 1)):
        shaft = poly([b(-2.4, -1, o)] + bezier(b(-2.4, -1, o), b(-2.6, 6, o), b(-2.6, 8, o), b(1, 8.5, o), 6)[1:] + [b(6.5, 8, o)] +
                     bezier(b(6.5, 8, o), b(7.5, 7.8, o), b(7.5, 6, o), b(5.5, 5.6, o), 4)[1:] + [b(2.4, 4.5, o), b(2.4, -1, o)])
        P.group(name, [(shaft, INK, INK, 0.8),
                       (taper([b(-1.8, 0.2, o), b(-1.9, 5.6, o)], 0.9, 0.9), CREAM, CREAM, 0.3),                                  # shaft stitch
                       (taper(bezier(b(4.2, 5.4, o), b(5.6, 5.6, o), b(6.6, 6.6, o), b(6.6, 7.6, o), 6), 0.9, 0.9), CREAM, CREAM, 0.3),   # toe cap
                       (ribbon([b(-2.5, 2.4, o), b(2.4, 2.4, o)], 0.9), CREAM, CREAM, 0.3),                                       # strap
                       (ribbon([b(-2.6, 6.6, o), b(-5.2, 6.6, o)], 1.0), CREAM, CREAM, 0.3)])                                     # spur bar
    r_sp = 2.8 * BS * CS
    for name in ('SPUR_L', 'SPUR_R'):
        P.group(name, [(ellipse(0, 0, 1.7 * BS * CS, 1.7 * BS * CS), INK, CREAM, 1.2)] +
                      [(ribbon([(math.cos(t) * r_sp, math.sin(t) * r_sp), (-math.cos(t) * r_sp, -math.sin(t) * r_sp)], 1.0), CREAM, CREAM, 0.3)
                       for t in (0, math.pi / 4, math.pi / 2, 3 * math.pi / 4)])

    # ---- horns: light tubes with ink ring joints, a dark tip; collars ----
    horn_spine = [(0, 0), (14, -3), (26, -7), (28, -17)]
    tip_spine = [(0, 0), (1, -4), (0, -8), (-2, -10)]
    for name, sd in (('HORN_L', -1), ('HORN_R', 1)):
        P.node(name, tube(horn_spine, 5.6, 2.6, sd, 14, HK * 1.1), PAPER, INK, 1.8)
    for name, sd in (('TIP_L', -1), ('TIP_R', 1)):
        P.node(name, tube(tip_spine, 2.6, 1.0, sd, 8, HK), INK, INK, 0.8)
    for name in ('COLLAR_L', 'COLLAR_R'):
        P.node(name, ellipse(0, 0, 1.5 * HK, 3.9 * HK), INK, CREAM, 1)
    P.group('RING', [(ellipse(0, 7, 6.5, 6), None, CREAM, 2.2), (ellipse(0, 13, 1.7, 1.7), CREAM, CREAM, 0.5)])
    P.group('STEAM', [('M-5,0 C-5,-7 5,-7 5,0 L4,0.4 C4,-5 -4,-5 -4,0.4 Z', None, PAPER, 1.5)] * 4)
    P.group('SPARK', [(star(0, 0, 3.2, 4, 0.3), CREAM, CREAM, 0.5)] * 6)
    P.node('PULSE', ellipse(0, 0, 3, 3), CREAM, CREAM, 0.5)
    P.group('TEAR', [('M0,-0.6 L18,-0.6 L18,0.6 L0,0.6 Z', WHITE_, WHITE_, 0.5)] * 8)
    P.node('KNOT', ellipse(0, 0, 2.4, 2.4), INK, CREAM, 1.5)

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
