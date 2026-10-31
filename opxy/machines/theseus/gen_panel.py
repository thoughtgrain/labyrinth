#!/usr/bin/env python3
"""panel.svg.fragment for theseus on the OP-XY: the PHALANX screen as retained artwork.

Static: the kit's shells, heads and stands, the drummer's cuirass. Moving parts are nodes that display.cpp places,
turns, scales, repaints or hides every frame: the five helmet heads (shown one at a time), the crest's bristles,
the eyes, the arms and sticks, the kick beater, the cymbals, ripple rings, sizzle specks and the sway arcs.
Greyscale: each control keeps the shape it had on the OP-1. The canvas is 480 x 220, art in 40..220.

    python3 gen_panel.py > panel.svg.fragment

Node map (keep in step with display.cpp):
  0..1    paths: ripple ring (reshaped: a ring of 64 points) and the LFO sway arc
  2..9    the two-page header's bar steps (SDK)
  10..14  the five heads: left profile, left 3/4, front, right 3/4, right profile (one shown)
  15..26  crest bristles (12, placed along the crown; the stalk is in each head)
  27, 28  eyes (one or two shown)                   29  eye rays (a star, shown on strong hits)
  30, 31  upper arms L, R   32, 33  forearms + sticks L, R (pivot at the elbow)
  34      kick beater (pivot at the pedal)          35  the thump diamond
  36, 37  hi-hat top, cymbal (wobble)               38..45  ripple rings (8, placed and scaled)
  46..53  sizzle specks                             54, 55  sway arcs
  56..63  spare
Copyright (c) 2026 briannhinton. MIT."""
K = 0.5523
SHELL, HEAD, EDGE, DEC, LITE = '#141414', '#1E1E1E', '#B9B9C9', '#625D77', '#F0F0F0'
HELM = '#2C2C2C'


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


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
    return 'M' + ' L'.join(f'{f(x)},{f(y)}' for x, y in pts) + (' Z' if close else '')


def ribbon(pts, w=0.5):
    out = [(x, y - w / 2) for x, y in pts] + [(x, y + w / 2) for x, y in reversed(pts)]
    return poly(out)


def sline(pts, col=DEC, w=1.5):
    return f'<path d="{poly(pts, False)}" fill="none" stroke="{col}" stroke-width="{w}"/>'


def node(i, d, fill, stroke, sw=1.5):
    return (f'<path id="theseus_node_{i}" transform="matrix(1 0 0 1 640 320)" d="{d}" fill="{fill if fill else "none"}" '
            f'stroke="{stroke}" stroke-width="{sw}"/>')


S = 1.45   # OP-1 screen units -> XY pixels


def main():
    o = ['<g id="theseus_panel">',
         '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    greys = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    for i, g in enumerate(greys):
        o.append(f'<path id="theseus_node_{2 + i}" transform="matrix(1 0 0 1 {60 * i} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" fill="{g}" stroke="{g}" stroke-width="0.5"/>')
    for i in range(4):
        o.append(f'<text id="theseus_text_{i}" transform="matrix(1 0 0 1 {5 + 120 * i} 10)" fill="{"#CBCBCB" if i < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for i in range(4):
        o.append(f'<text id="theseus_text_{4 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 17.4)" fill="{"#CBCBCB" if i < 2 else "#000000"}" font-family="UniversNextPro-Light" font-size="20px">-</text>')
    for i in range(4):
        o.append(f'<text id="theseus_text_{8 + i}" transform="matrix(1 0 0 1 {5 + 120 * i} 33)" fill="#5A5A5A" font-family="UniversNextPro-Light" font-size="10px">-</text>')
    for i in range(4):
        o.append(f'<text id="theseus_text_{12 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 33)" fill="#707070" font-family="UniversNextPro-Light" font-size="13px">-</text>')

    # ---- static kit, OP-1 layout scaled by S and shifted so the kit sits in 50..215 ----
    def P(x, y):
        return (x * S + 8, (y - 8) * S + 50)
    # stands
    for a, b in (((94, 80), (94, 132)), ((232, 69), (234, 131)), ((122, 107), (122, 131)), ((197, 120), (195, 140)), ((217, 120), (219, 140)), ((143, 126), (136, 140)), ((177, 126), (184, 140))):
        o.append(sline([P(*a), P(*b)]))
    for c in ((87, 140), (94, 131), (101, 140)), ((227, 140), (234, 130), (241, 140)), ((115, 140), (122, 130), (129, 140)):
        o.append(sline([P(*p) for p in c]))
    # the drummer's cuirass
    cx, cy = P(158, 95)
    o.append(f'<path d="M{f(cx - 25)},{f(cy)} C{f(cx - 22)},{f(cy - 9)} {f(cx - 12)},{f(cy - 12)} {f(cx)},{f(cy - 12)} C{f(cx + 12)},{f(cy - 12)} {f(cx + 22)},{f(cy - 9)} {f(cx + 25)},{f(cy)} L{f(cx + 18)},{f(cy + 16)} L{f(cx - 18)},{f(cy + 16)} Z" fill="{SHELL}" stroke="{DEC}" stroke-width="1.5"/>')
    # drums: snare, floor tom, rack tom; then the kick
    for dcx, dcy, rx, ry, h in ((122, 98, 14, 4, 8), (207, 108, 13, 4, 16), (190, 88, 11, 3.5, 8)):
        x, y = P(dcx, dcy)
        o.append(f'<path d="{shell(x, y, rx * S, ry * S, h * S)}" fill="{SHELL}" stroke="{EDGE}" stroke-width="1.5"/>')
        o.append(f'<path d="{ellipse(x, y, rx * S, ry * S)}" fill="{HEAD}" stroke="{EDGE}" stroke-width="1.5"/>')
    o.append(sline([P(176, 99), P(186, 96)], EDGE))
    kx, ky = P(160, 114)
    o.append(f'<path d="{ellipse(kx, ky, 22 * S, 22 * S)}" fill="{SHELL}" stroke="{EDGE}" stroke-width="2"/>')
    o.append(f'<path d="{ellipse(kx, ky, 18.5 * S, 18.5 * S)}" fill="none" stroke="{DEC}" stroke-width="1"/>')
    # the pedal's footboard (static)
    o.append(sline([P(158, 141.5), P(174, 139.5)], LITE, 2))

    # ---- mutable outlines: 0 the ripple ring, 1 the sway arc ----
    import math
    for i in range(2):
        pts = [(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]
        o.append(node(i, poly(pts), None, LITE, 1.5))

    # ---- the five heads: helmet units (the OP-1's), origin at the neck pivot; scaled 1.1 x S on screen ----
    def bez(p0, p1, p2, p3):
        return f'C{f(p1[0])},{f(p1[1])} {f(p2[0])},{f(p2[1])} {f(p3[0])},{f(p3[1])}'
    def profile(sign):   # facing +x for sign 1; mirrored for -1
        m = lambda p: (p[0] * sign, p[1])
        d = f'M{f(-16 * sign)},14 ' + bez(m((-16, 14)), m((-20, 4)), m((-21, -10)), m((-14, -18))) + ' ' + bez(m((-14, -18)), m((-8, -24)), m((6, -25)), m((13, -18))) + ' ' + bez(m((13, -18)), m((17, -14)), m((19, -9)), m((19.5, -4)))
        d += f' L{f(21 * sign)},9 L{f(17 * sign)},10.5 L{f(14 * sign)},4.5 L{f(12.5 * sign)},8 ' + bez(m((12.5, 8)), m((15, 12)), m((18, 16)), m((19, 20))) + f' L{f(12 * sign)},22 ' + bez(m((12, 22)), m((6, 22)), m((0, 18)), m((-4, 16))) + ' ' + bez(m((-4, 16)), m((-8, 18)), m((-12, 18)), m((-16, 14))) + ' Z'
        # the face opening: eye slot and the cheek gap, as holes drawn in the shell colour on top (separate subpaths, fill rule nonzero: so they are drawn as a second node? no: keep them in the same path and rely on even-odd? The XY's renderer fills nonzero. Draw them in display as a dark node instead.)
        return d
    def three_quarter(sign):
        m = lambda p: (p[0] * sign, p[1])
        d = f'M{f(-19 * sign)},-3 ' + bez(m((-19, -3)), m((-19.5, -17)), m((-10, -24.5)), m((1, -24))) + ' ' + bez(m((1, -24)), m((12, -23.5)), m((18.5, -16)), m((18.5, -3)))
        d += f' L{f(19 * sign)},7 ' + bez(m((19, 7)), m((18.5, 15)), m((15, 20)), m((9, 22.5))) + f' L{f(6 * sign)},22.5 L{f(6 * sign)},9 L{f(3 * sign)},9 L{f(3 * sign)},23 L{f(-3 * sign)},22 ' + bez(m((-3, 22)), m((-10, 21)), m((-16, 18)), m((-18, 10))) + ' Z'
        return d
    def front():
        d = 'M-19.5,-4 ' + bez((-19.5, -4), (-19.5, -17), (-11, -24), (0, -24)) + ' ' + bez((0, -24), (11, -24), (19.5, -17), (19.5, -4))
        d += ' L20,6 ' + bez((20, 6), (19, 16), (14, 21), (7, 23)) + ' L3,23 L3,9 L-3,9 L-3,23 L-7,23 ' + bez((-7, 23), (-14, 21), (-19, 16), (-20, 6)) + ' Z'
        return d
    heads = [profile(-1), three_quarter(-1), front(), three_quarter(1), profile(1)]
    for i, d in enumerate(heads):
        o.append(node(10 + i, d, HELM, EDGE, 1.5))
    # the face openings, one per head, drawn dark over the head (the renderer fills nonzero, so holes are nodes)
    # they are static shapes in head units; display.cpp places them with the head. nodes 56..60
    def slot_profile(sign):
        m = lambda p: (p[0] * sign, p[1])
        s1 = poly([m(p) for p in ((6.5, -7), (11, -10), (19.6, -9.6), (19.6, -5), (11, -4.4))])
        s2 = poly([m(p) for p in ((14.6, -5), (17, -5), (15.6, 4.8), (13.4, 4.8))])
        return s1 + ' ' + s2
    def slot_tq(sign):
        m = lambda p: (p[0] * sign, p[1])
        return poly([m(p) for p in ((6, -9.6), (16.5, -9), (17.5, -4.5), (6, -4.2))]) + ' ' + poly([m(p) for p in ((-9, -8.5), (3, -9.6), (3, -4.2), (-8.5, -4.2))]) + ' ' + poly([m(p) for p in ((3, 9), (6, 9), (6, 22.5), (3, 23))])
    def slot_front():
        return poly([(15.5, -9), (3, -10), (3, -4.5), (14.5, -4)]) + ' ' + poly([(-15.5, -9), (-3, -10), (-3, -4.5), (-14.5, -4)]) + ' ' + poly([(-3, 9), (3, 9), (3, 23), (-3, 23)])
    slots = [slot_profile(-1), slot_tq(-1), slot_front(), slot_tq(1), slot_profile(1)]
    for i, d in enumerate(slots):
        o.append(node(56 + i, d, '#060608', '#060608', 0.5))

    # ---- crest bristles (15..26): a unit bristle pointing -y, placed along the crown ----
    for i in range(12):
        o.append(node(15 + i, 'M-0.7,0 L0.7,0 L0.5,-8 L-0.5,-8 Z', LITE, LITE, 0.5))
    # ---- eyes (27, 28): a disc; rays (29): six spokes ----
    o.append(node(27, ellipse(0, 0, 2, 2), LITE, LITE, 0.5))
    o.append(node(28, ellipse(0, 0, 2, 2), LITE, LITE, 0.5))
    rays = ' '.join(ribbon([(math.cos(a) * 3.5, math.sin(a) * 3.5), (math.cos(a) * 7, math.sin(a) * 7)], 1) for a in [k * math.pi / 3 + math.pi / 6 for k in range(6)])
    o.append(node(29, rays, LITE, LITE, 0.5))
    # ---- arms: upper arm (30, 31) pivot at the shoulder pointing +x; forearm + stick (32, 33) pivot at the elbow ----
    for i in (30, 31):
        o.append(node(i, ribbon([(0, 0), (30, 0)], 5), None, EDGE, 1.5))
    for i in (32, 33):
        o.append(node(i, ribbon([(0, 0), (26, 0)], 4) + ' ' + ribbon([(22, 0), (58, 0)], 2.4) + ' ' + ellipse(58, 0, 1.8, 1.8), None, EDGE, 1.5))
    # ---- kick beater (34): pivot at the pedal, pointing -y; the thump diamond (35) ----
    o.append(node(34, ribbon([(0, 0), (0, -24)], 2) + ' ' + ellipse(0, -24, 3.5, 3.5), LITE, LITE, 1))
    o.append(node(35, 'M-7,0 L0,-7 L7,0 L0,7 Z', None, LITE, 1))
    # ---- hi-hat top (36) and cymbal (37): rings with a bell ----
    def cym(rx, ry):
        return ellipse(0, 0, rx, ry) + ' M-5,-1 C-5,-5 5,-5 5,-1 L4,-0.6 C4,-4 -4,-4 -4,-0.6 Z'
    o.append(node(36, cym(13 * S, 3 * S), None, EDGE, 1.5))
    o.append(node(37, cym(17 * S, 3 * S), None, EDGE, 1.5))
    hx, hy = P(94, 77)
    o.append(f'<path d="{ellipse(hx, hy + 4, 13 * S, 2.6 * S)}" fill="none" stroke="{DEC}" stroke-width="1.5"/>')   # the bottom hat, static
    # ---- ripple rings (38..45): unit rings (scaled per frame) ----
    for i in range(8):
        o.append(node(38 + i, ellipse(0, 0, 16, 16), None, LITE, 1))
    # ---- sizzle specks (46..53) ----
    for i in range(8):
        o.append(node(46 + i, 'M-1,-1 L1,-1 L1,1 L-1,1 Z', LITE, LITE, 0.5))
    # ---- sway arcs (54, 55): placed per frame ----
    for i in (54, 55):
        o.append(node(i, ribbon([(math.cos(a) * 30, math.sin(a) * 30) for a in [-0.5 + k * 0.1 for k in range(11)]], 1.2), None, LITE, 1))
    for i in range(61, 64):
        o.append(node(i, 'M-1,-1 L1,-1 L1,1 L-1,1 Z', None, DEC, 1))

    # ---- palette: 0..7 near-black fill with an outline dim..white; 8..15 solid greys ----
    outlines = ['#2A2A2A', '#3C3C3C', '#5A5A5A', '#707070', '#8C8C8C', '#A8A8A8', '#CBCBCB', '#F0F0F0']
    for i, s in enumerate(outlines):
        o.append(f'<circle id="theseus_palette_{i}" cx="640" cy="0" r="1" fill="#1C1C1C" stroke="{s}" stroke-width="1.5"/>')
    solids = ['#060608', '#2A2A2A', '#4B4B4B', '#707070', '#9A9A9A', '#B8B8B8', '#D6D6D6', '#FFFFFF']
    for i, s in enumerate(solids):
        o.append(f'<circle id="theseus_palette_{8 + i}" cx="640" cy="0" r="1" fill="{s}" stroke="{s}" stroke-width="1.5"/>')
    o.append('</g>')
    print('\n'.join(o))


if __name__ == '__main__':
    main()
