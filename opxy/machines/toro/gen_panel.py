#!/usr/bin/env python3
"""panel.svg.fragment for toro on the OP-XY: the RIDER screen as retained artwork.

The OP-XY draws the artwork itself; display.cpp only moves, turns, scales, repaints or hides each part every
frame. So every part that moves is its own element with a node id, authored around its own pivot at (0, 0),
and parts that do not move are plain paths. Greyscale only: the OP-XY screen has no colour, so each control
keeps the shape it had on the OP-1 (horns, lasso, eye slits, hose pulse) and its grey level does the rest.
The canvas is 480 x 220 with the two-page header on top; the art sits in 40..220.

Everything is authored in the OP-1's rig units (gen_svg.py / toro_screen.c): origin on the ram's top, the head
1.17x about (0, -30), the cowboy 1.55x about his seat (0, -52), his hat 1.7x about (0, -87) and his boots 1.75x
about the ankle. display.cpp scales the lot by RIG. Limbs are authored pointing +x from their joint (the left
side mirrored), so a turn per frame bends them.

    python3 gen_panel.py > panel.svg.fragment

Node map (keep in step with display.cpp):
  0..1    paths: the lasso loop and the rope from hand to knot (64 points each, reshaped per frame)
  2..9    the two-page header's bar steps (SDK)
  10      shadow                              11      rod (the ram)
  12      body: barrel, seams, ears, handle   13, 14  horns L, R           15, 16  horn tips       17, 18  horn root rings
  19      faceplate + muzzle                  20      faceplate decals (brow and cheek lines)   21  vents
  22, 23  eye slits L, R                      24, 25  slit highlights L, R (lit on a note)
  26      nose ring                           27..30  steam puffs          31..36  sparks (NOIS)   37  hose pulse
  38      cowboy torso + rein arm             39      belt, buckle, bandana crease
  40      his head                            41      his face (eyes, brows, mouth; scaled wide when thrown)
  42      hat brim                            43      hat crown
  44      lasso upper arm                     45      lasso forearm + hand
  46, 47  thighs L, R                         48, 49  shins L, R           50, 51  spur wheels L, R
  52..59  APHX tear slices                    60      the lasso knot       61, 62  boots L, R   63  spare
Copyright (c) 2026 briannhinton. MIT."""
import math

K = 0.5523
BG, EDGE, SEC, DEC, LITE, MID, DARK = '#000000', '#CBCBCB', '#9A9A9A', '#5A5A5A', '#F0F0F0', '#707070', '#2A2A2A'
BODY, PLATE, MUZ = '#121212', '#1C1C1C', '#0E0E0E'
HK, HY = 1.17, -30            # the head's scale and lift
CS, CY = 1.55, -52            # the cowboy's scale about his seat
HS, HATY = 1.7, -87           # his hat's scale about the crown's base
BS = 1.75                     # his boots' scale about the ankle


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
    """a polyline as a thin closed ribbon (the XY needs every outline to enclose area), offset along the normal"""
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


def bez(p0, p1, p2, p3):
    return f'M{pt(p0)} C{pt(p1)} {pt(p2)} {pt(p3)}'


def bez_ribbon(p0, p1, p2, p3, w, n=10):
    """a cubic as a thin closed ribbon"""
    pts = []
    for i in range(n + 1):
        u = i / n; v = 1 - u
        pts.append((v ** 3 * p0[0] + 3 * v * v * u * p1[0] + 3 * v * u * u * p2[0] + u ** 3 * p3[0],
                    v ** 3 * p0[1] + 3 * v * v * u * p1[1] + 3 * v * u * u * p2[1] + u ** 3 * p3[1]))
    return ribbon(pts, w)


def limb(length, w0, w1, sign=1):
    """a tapered limb from (0,0) along sign*x with round ends"""
    a = []
    for i in range(7):                            # the root cap
        t = math.pi / 2 + math.pi * i / 6
        a.append((math.cos(t) * w0 / 2 * sign, -math.sin(t) * w0 / 2))
    b = []
    for i in range(7):                            # the far cap
        t = -math.pi / 2 + math.pi * i / 6
        b.append((length * sign + math.cos(t) * w1 / 2 * sign, math.sin(t) * w1 / 2))
    return poly(a + b)


def h(x, y):                                      # head-local -> rig units
    return (x * HK, y * HK + HY)


def c(x, y):                                      # cowboy-local -> rig units relative to his seat
    return (x * CS, (y - CY) * CS)


def node(i, d, fill, stroke, sw=1.5, x=0, y=0, extra=''):
    fl = fill if fill else 'none'
    return (f'<path id="toro_node_{i}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" d="{d}" fill="{fl}" '
            f'stroke="{stroke}" stroke-width="{sw}"{extra}/>')


def main():
    o = ['<g id="toro_panel">',
         '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    for i, g in enumerate(greys):
        o.append(f'<path id="toro_node_{2 + i}" transform="matrix(1 0 0 1 {60 * i} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" fill="{g}" stroke="{g}" stroke-width="0.5"/>')
    for i in range(4):
        o.append(f'<text id="toro_text_{i}" transform="matrix(1 0 0 1 {5 + 120 * i} 10)" fill="{"#CBCBCB" if i < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for i in range(4):
        o.append(f'<text id="toro_text_{4 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 17.4)" fill="{"#CBCBCB" if i < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
    for i in range(4):
        o.append(f'<text id="toro_text_{8 + i}" transform="matrix(1 0 0 1 {5 + 120 * i} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for i in range(4):
        o.append(f'<text id="toro_text_{12 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')

    # ---- static floor: the mat, the hose (a FOLD pulse runs along it) and the turntable ----
    cx0, base = 240, 208
    o.append(f'<path d="{ellipse(cx0, base + 6, 118, 8)}" fill="none" stroke="{DARK}" stroke-width="1.5"/>')
    o.append(f'<path d="M{cx0 + 30},{base + 6} C300,{base + 14} 360,{base + 8} 400,{base - 2}" fill="none" stroke="{MID}" stroke-width="1.5"/>')
    o.append(f'<path d="M400,{base - 2} L408,{base - 2}" fill="none" stroke="{DEC}" stroke-width="1.5"/>')
    # ---- shadow (10): a disc, scaled per frame ----
    o.append(node(10, ellipse(0, 0, 30, 5), '#141414', '#141414', 0.5, 640, 320))
    # the turntable base and cylinder (static, drawn over the shadow)
    o.append(f'<path d="{ellipse(cx0, base, 30, 5)}" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')
    o.append(f'<path d="M{cx0 - 9},{base - 2} L{cx0 + 9},{base - 2} L{cx0 + 9},{base - 17} L{cx0 - 9},{base - 17} Z" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')
    # ---- rod (11): a unit square mapped onto the ram (scaled/turned per frame), 16 px at scale 1 ----
    o.append(node(11, 'M-4,0 L4,0 L4,-16 L-4,-16 Z', '#9A9A9A', SEC, 1, 640, 320))

    # ---- the bull's body (12): barrel, side seams, ears and the handle, behind the cowboy ----
    barrel = 'M-36,-40 C-26,-50 26,-50 36,-40 C47,-28 47,-8 34,-2 L-34,-2 C-47,-8 -47,-28 -36,-40 Z'
    seams = ' '.join(bez_ribbon((30 * sd, -44), (40 * sd, -36), (42 * sd, -18), (34 * sd, -4), 0.9, 8) for sd in (-1, 1))
    handle = bez_ribbon((-6, -52), (-6, -64), (6, -64), (6, -52), 1.6, 8)
    ears = ' '.join(poly([h(x * sd, y) for x, y in [(25, -12), (41, -11), (44, -6), (40, -2), (25, -2)]]) for sd in (-1, 1))
    o.append(node(12, ' '.join([barrel, seams, ears, handle]), BODY, SEC, 1.5, 640, 320))

    # ---- the cowboy (behind the faceplate; his torso and hat rise above it) ----
    # torso (38): the OP-1's bottle shape, with the rein arm gripping the handle; pivot at the seat
    torso = (f'M{pt(c(-6, -54))} C{pt(c(-9.5, -73))} {pt(c(-6, -75))} {pt(c(0, -75))} C{pt(c(6, -75))} {pt(c(9.5, -73))} {pt(c(6, -54))} Z')
    rein = ribbon([c(-8.6, -71), c(-14, -63), c(-5, -61)], 1.9 * CS)
    rein_hand = ellipse(*c(-4.2, -61), 1.7 * CS, 1.7 * CS)
    o.append(node(38, ' '.join([torso, rein, rein_hand]), BODY, SEC, 1.5, 640, 320))
    # belt, buckle and the bandana's crease (39): dim lines over the torso
    belt = ribbon([c(-6.3, -57.5), c(6.3, -57.5)], 0.8)
    buckle = poly([c(-2.2, -59.2), c(2.2, -59.2), c(2.2, -55.8), c(-2.2, -55.8)])
    bandana = poly([c(-5, -74), c(5, -74), c(0, -68.5)])
    crease = ribbon([c(0, -68), c(0, -60)], 0.8)
    o.append(node(39, ' '.join([belt, buckle, bandana, crease]), None, DEC, 1, 640, 320))
    # head (40): a ring; face (41): eyes, brows, mouth, pivot at the head centre so a scale widens the eyes
    hx, hy = c(0, -81.5)
    o.append(node(40, ellipse(hx, hy, 6.6 * CS, 6.6 * CS), BODY, SEC, 1.5, 640, 320))
    eyes = ' '.join(ellipse(2.5 * sd * CS, -1.1 * CS, 1.0 * CS, 1.0 * CS) for sd in (-1, 1))
    brows = ' '.join(ribbon([(1.2 * sd * CS, -3.5 * CS), (3.8 * sd * CS, -2.7 * CS)], 0.8) for sd in (-1, 1))
    mouth = bez_ribbon((-2.6 * CS, 2.7 * CS), (-1.3 * CS, 4.2 * CS), (1.3 * CS, 4.2 * CS), (2.6 * CS, 2.7 * CS), 1.0, 6)
    o.append(node(41, ' '.join([eyes, brows, mouth]), None, LITE, 1.2, 640, 320))
    # hat: brim (42) a lens, pivot at the crown's base; crown (43) the dome and its band, same pivot
    def hs(x, y):
        return (x * HS * CS, (y - HATY) * HS * CS)
    brim = (f'M{pt(hs(-16, -86.5))} C{pt(hs(-12, -83.8))} {pt(hs(12, -83.8))} {pt(hs(16, -86.5))} '
            f'C{pt(hs(12, -86))} {pt(hs(-12, -86))} {pt(hs(-16, -86.5))} Z')
    o.append(node(42, brim, "#161616", LITE, 1.2, 640, 320))
    crown = (f'M{pt(hs(-7, -85.6))} C{pt(hs(-8, -92))} {pt(hs(-6.5, -96.5))} {pt(hs(-3, -96))} C{pt(hs(-1, -94.6))} {pt(hs(1, -94.6))} {pt(hs(3, -96))} '
             f'C{pt(hs(6.5, -96.5))} {pt(hs(8, -92))} {pt(hs(7, -85.6))} Z')
    band = ribbon([hs(-7.3, -88), hs(7.3, -88)], 0.8)
    o.append(node(43, ' '.join([crown, band]), '#161616', LITE, 1.5, 640, 320))
    # lasso arm: upper arm (44) from the shoulder, forearm with the hand ring (45) from the elbow, both along +x
    o.append(node(44, limb(9 * CS, 1.9 * CS, 1.7 * CS), BODY, SEC, 1.5, 640, 320))
    o.append(node(45, ' '.join([limb(10 * CS, 1.7 * CS, 1.6 * CS), ellipse(10 * CS, 0, 1.8 * CS, 1.8 * CS)]), BODY, SEC, 1.5, 640, 320))
    # ---- the faceplate (19): the OP-1's twelve-sided plate with the muzzle, in front of the cowboy ----
    head = [(-20, -22), (20, -22), (27, -10), (24, 4), (17, 16), (14, 28), (8, 32), (-8, 32), (-14, 28), (-17, 16), (-24, 4), (-27, -10)]
    muzzle = [(-12, 13), (12, 13), (14, 24), (9, 31), (-9, 31), (-14, 24)]
    o.append(node(19, poly([h(*p) for p in head]), PLATE, EDGE, 2, 640, 320))
    o.append(node(20, ' '.join([poly([h(*p) for p in muzzle])] + [ribbon([h(27 * sd, -10), h(10 * sd, -7)], 0.9) for sd in (-1, 1)]
                                + [ribbon([h(24 * sd, 4), h(14 * sd, 13)], 0.7) for sd in (-1, 1)]), MUZ, DEC, 1.2, 640, 320))
    # vents (21): two dark slots in the muzzle
    o.append(node(21, ' '.join(poly([h(x * sd, y) for x, y in [(3.5, 18), (9, 18), (8, 23), (4, 23)]]) for sd in (-1, 1)), '#000000', DEC, 0.8, 640, 320))
    # eye slits (22, 23): the OP-1's quads, pivot at each slit's middle; a scale opens them. Highlights (24, 25) inside.
    for i, sd in ((22, -1), (23, 1)):
        q = [(21, -9), (8, -5.2), (8, 1.5), (20, -1.8)]
        mx, my = sum(x for x, _ in q) / 4, sum(y for _, y in q) / 4
        o.append(node(i, poly([((x - mx) * sd * HK, (y - my) * HK) for x, y in q]), '#000000', LITE, 1.2, 640, 320))
    for i, sd in ((24, -1), (25, 1)):
        o.append(node(i, poly([(x * sd * HK, y * HK) for x, y in [(-4.5, -1.2), (4.5, -3.4), (4.5, -1.6), (-4.5, 0.4)]]), LITE, LITE, 0.3, 640, 320))
    # thighs (46, 47): from the hip along +x (right) / -x (left), 17.7 cowboy units
    o.append(node(46, limb(17.7 * CS, 2.6 * CS, 2.0 * CS, -1), BODY, SEC, 1.5, 640, 320))
    o.append(node(47, limb(17.7 * CS, 2.6 * CS, 2.0 * CS, 1), BODY, SEC, 1.5, 640, 320))
    # shins + boots (48, 49): the shin from the knee along +x/-x, 15 cowboy units, then the OP-1's boot at the ankle
    def boot(o_):
        def b(x, y):
            return (x * BS * CS, y * BS * CS)
        shaft = [b(-2.4 * o_, -1)]
        p = [b(-2.6 * o_, 6), b(-2.6 * o_, 8), b(1 * o_, 8.5), b(6.5 * o_, 8)]
        q = [b(6.5 * o_, 8), b(7.5 * o_, 7.8), b(7.5 * o_, 6), b(5.5 * o_, 5.6)]
        d = (f'M{pt(shaft[0])} C{pt(p[0])} {pt(p[1])} {pt(p[2])} L{pt(p[3])} C{pt(q[1])} {pt(q[2])} {pt(q[3])} '
             f'L{pt(b(2.4 * o_, 4.5))} L{pt(b(2.4 * o_, -1))} Z')
        strap = ribbon([b(-2.6 * o_, 2.4), b(2.4 * o_, 2.4)], 0.7)
        spur_bar = ribbon([b(-2.6 * o_, 6.5), b(-5.0 * o_, 6.5)], 0.7)
        return ' '.join([d, strap, spur_bar])
    for i, sd in ((48, -1), (49, 1)):
        o.append(node(i, limb(15 * CS, 2.0 * CS, 1.8 * CS, sd), BODY, SEC, 1.5, 640, 320))
    # boots (61, 62): hang from the ankle, upright in his frame (the OP-1 scaled them about the ankle, unturned)
    for i, sd in ((61, -1), (62, 1)):
        o.append(node(i, boot(sd), BODY, SEC, 1.5, 640, 320))
    # spur wheels (50, 51): a ring with four spokes, pivot at the hub; spun per frame
    spokes = ' '.join(ribbon([(math.cos(t) * 2.8 * BS * CS, math.sin(t) * 2.8 * BS * CS), (-math.cos(t) * 2.8 * BS * CS, -math.sin(t) * 2.8 * BS * CS)], 0.7)
                      for t in (0, math.pi / 4, math.pi / 2, 3 * math.pi / 4))
    for i in (50, 51):
        o.append(node(i, ' '.join([ellipse(0, 0, 1.7 * BS * CS, 1.7 * BS * CS), spokes]), None, EDGE, 1, 640, 320))

    # ---- horns (13..18): one curved tapered tube each (the OP-1's Bezier spine), a curling tip on its end, and
    # a joint ring at the root. +x for the right; the left is authored mirrored. 15/16 the tips, 17/18 the rings ----
    def tube(spine, w0, w1, sign, n=14):
        pts = []
        for i in range(n + 1):
            u = i / n; v = 1 - u
            pts.append((sign * (v ** 3 * spine[0][0] + 3 * v * v * u * spine[1][0] + 3 * v * u * u * spine[2][0] + u ** 3 * spine[3][0]) * HK,
                        (v ** 3 * spine[0][1] + 3 * v * v * u * spine[1][1] + 3 * v * u * u * spine[2][1] + u ** 3 * spine[3][1]) * HK))
        up, dn = [], []
        for i, (x, y) in enumerate(pts):
            j = min(i, n - 1)
            dx, dy = pts[j + 1][0] - pts[j][0], pts[j + 1][1] - pts[j][1]
            ln = math.hypot(dx, dy) or 1
            w = (w0 + (w1 - w0) * (i / n) ** 1.3) * HK / 2
            up.append((x - dy / ln * w, y + dx / ln * w)); dn.append((x + dy / ln * w, y - dx / ln * w))
        return poly(up + list(reversed(dn)))
    horn_spine = [(0, 0), (14, -3), (26, -7), (28, -17)]
    tip_spine = [(0, 0), (1, -4), (0, -8), (-2, -10)]
    for i, sd in ((13, -1), (14, 1)):
        o.append(node(i, tube(horn_spine, 5.6, 2.6, sd), '#9A9A9A', '#9A9A9A', 1, 640, 320))
    for i, sd in ((15, -1), (16, 1)):
        o.append(node(i, tube(tip_spine, 2.6, 0.9, sd, 8), '#D6D6D6', '#D6D6D6', 1, 640, 320))
    for i, sd in ((17, -1), (18, 1)):
        o.append(node(i, ellipse(0, 0, 1.2 * HK, 3.6 * HK), None, LITE, 1.2, 640, 320))
    # ---- nose ring (26): hangs from its pivot ----
    o.append(node(26, ellipse(0, 7, 6.5, 6), None, LITE, 2, 640, 320))
    # ---- steam puffs (27..30) ----
    for i in range(4):
        o.append(node(27 + i, 'M-5,0 C-5,-7 5,-7 5,0 L4,0.4 C4,-5 -4,-5 -4,0.4 Z', None, SEC, 1.5, 640, 320))
    # ---- sparks (31..36): small crosses ----
    for i in range(6):
        o.append(node(31 + i, 'M-3,-0.3 L3,-0.3 L3,0.3 L-3,0.3 Z M-0.3,-3 L0.3,-3 L0.3,3 L-0.3,3 Z', LITE, LITE, 0.5, 640, 320))
    # ---- hose pulse (37): a disc ----
    o.append(node(37, ellipse(0, 0, 3, 3), LITE, LITE, 0.5, 640, 320))
    # ---- the two mutable outlines: 0 the lasso loop, 1 the rope ----
    for i in range(2):
        pts = [(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]
        o.append(node(i, poly(pts), None, LITE, 1.5, 640, 320))
    # APHX tear slices (52..59): short bright bars, placed per frame
    for i in range(8):
        o.append(node(52 + i, 'M0,-0.6 L18,-0.6 L18,0.6 L0,0.6 Z', LITE, LITE, 0.5, 640, 320))
    # the lasso knot (60) and spares
    o.append(node(60, ellipse(0, 0, 2.2, 2.2), None, LITE, 1.5, 640, 320))
    for i in range(63, 64):
        o.append(node(i, 'M-1,-1 L1,-1 L1,1 L-1,1 Z', None, DEC, 1, 640, 320))

    # ---- palette: 0..7 a near-black fill with an outline from dim to white (bodies), 8..15 solid greys (lit parts) ----
    outlines = ['#2A2A2A', '#3C3C3C', '#5A5A5A', '#707070', '#8C8C8C', '#A8A8A8', '#CBCBCB', '#F0F0F0']
    for i, s in enumerate(outlines):
        o.append(f'<circle id="toro_palette_{i}" cx="640" cy="0" r="1" fill="#101010" stroke="{s}" stroke-width="1.5"/>')
    solids = ['#1A1A1A', '#2A2A2A', '#4B4B4B', '#707070', '#9A9A9A', '#B8B8B8', '#D6D6D6', '#FFFFFF']
    for i, s in enumerate(solids):
        o.append(f'<circle id="toro_palette_{8 + i}" cx="640" cy="0" r="1" fill="{s}" stroke="{s}" stroke-width="1.5"/>')
    o.append('</g>')
    print('\n'.join(o))


def translate(d, dx, dy):
    """shift every coordinate pair in a path string"""
    import re
    def rep(m):
        return f'{f(float(m.group(1)) + dx)},{f(float(m.group(2)) + dy)}'
    return re.sub(r'(-?[\d.]+),(-?[\d.]+)', rep, d)


if __name__ == '__main__':
    main()
