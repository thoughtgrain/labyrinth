#!/usr/bin/env python3
"""panel.svg.fragment for toro on the OP-XY: the RIDER screen as retained artwork.

The OP-XY draws the artwork itself; display.cpp only moves, turns, scales, repaints or hides each part every
frame. So every part that moves is its own element with a node id, authored around its own pivot at (0, 0),
and parts that do not move are plain paths. Greyscale only: the OP-XY screen has no colour, so each control
keeps the shape it had on the OP-1 (horns, lasso, eye slits, hose pulse) and its grey level does the rest.
The canvas is 480 x 220 with the two-page header on top; the art sits in 40..220.

    python3 gen_panel.py > panel.svg.fragment

Node map (keep in step with display.cpp):
  0..1    paths: the lasso loop (64 points, reshaped per frame) and the FOLD ripple (unused outline kept flat)
  2..9    the two-page header's bar steps (SDK)
  10      shadow                                    11  rod (the ram)
  12      rig: body + faceplate (one group pivot)   13, 14  horns L, R (pivot at the root)
  15, 16  eye slits L, R                            17  nose ring
  18, 19  eye glow L, R                             20..23  steam puffs
  24..29  sparks (NOIS)                             30  hose pulse
  31      cowboy torso + head group                 32  hat          33, 34  legs L, R (pivot at the hip)
  35      lasso arm                                 36..43  APHX tear slices
  44      rig facets and vents (rides on the rig)   45  the cowboy's face   46  the body and ears   47  spare
Copyright (c) 2026 briannhinton. MIT."""
import math

K = 0.5523
BG, EDGE, SEC, DEC, LITE, MID, DARK = '#000000', '#CBCBCB', '#9A9A9A', '#5A5A5A', '#F0F0F0', '#707070', '#2A2A2A'
S = 1.5   # the OP-1 rig was drawn in 320x160 units; the XY art is 1.5x


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def ellipse(cx, cy, rx, ry):
    return (f'M{f(cx + rx)},{f(cy)} C{f(cx + rx)},{f(cy + K * ry)} {f(cx + K * rx)},{f(cy + ry)} {f(cx)},{f(cy + ry)} '
            f'C{f(cx - K * rx)},{f(cy + ry)} {f(cx - rx)},{f(cy + K * ry)} {f(cx - rx)},{f(cy)} '
            f'C{f(cx - rx)},{f(cy - K * ry)} {f(cx - K * rx)},{f(cy - ry)} {f(cx)},{f(cy - ry)} '
            f'C{f(cx + K * rx)},{f(cy - ry)} {f(cx + rx)},{f(cy - K * ry)} {f(cx + rx)},{f(cy)} Z')


def poly(pts, close=True):
    return 'M' + ' L'.join(f'{f(x)},{f(y)}' for x, y in pts) + (' Z' if close else '')


def ribbon(pts, w=0.5):
    """a polyline as a thin closed ribbon (the XY needs every outline to enclose area)"""
    out = [(x, y - w / 2) for x, y in pts] + [(x, y + w / 2) for x, y in reversed(pts)]
    return poly(out)


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

    # ---- static floor: the mat and the hose (a FOLD pulse runs along it) ----
    cx0, base = 240, 196
    o.append(f'<path d="{ellipse(cx0, base + 6, 120, 8)}" fill="none" stroke="{DARK}" stroke-width="1.5"/>')
    o.append(f'<path d="M{cx0 + 30},{base + 6} C300,{base + 14} 360,{base + 8} 400,{base - 2}" fill="none" stroke="{MID}" stroke-width="1.5"/>')
    o.append(f'<path d="M400,{base - 2} L408,{base - 2}" fill="none" stroke="{DEC}" stroke-width="1.5"/>')
    # the turntable base (static)
    o.append(f'<path d="{ellipse(cx0, base, 36, 7)}" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')
    o.append(f'<path d="M{cx0 - 12},{base - 4} L{cx0 + 12},{base - 4} L{cx0 + 12},{base - 18} L{cx0 - 12},{base - 18} Z" fill="#1A1A1A" stroke="{SEC}" stroke-width="1.5"/>')

    # ---- shadow (10): a unit disc, scaled per frame ----
    o.append(node(10, ellipse(0, 0, 30, 5), '#141414', '#141414', 0.5, 640, 320))      # shadow at its middle size
    # ---- rod (11): a unit square mapped onto the ram (scaled/turned per frame) ----
    o.append(node(11, 'M-7,0 L7,0 L7,-16 L-7,-16 Z', '#1A1A1A', SEC, 1.5, 640, 320))      # the ram at its resting height

    # ---- the bull's body and ears (46): behind the cowboy; the faceplate (12) is in front of him ----
    o.append(node(46, ' '.join([ellipse(0, -8, 46, 22), 'M-30,-26 L-44,-44 L-32,-40 Z M30,-26 L44,-44 L32,-40 Z']), '#161616', SEC, 1.5, 640, 320))
    # ---- the cowboy ----
    # torso + head + rein arm (31): pivot at the seat, one compound path
    torso = 'M-10,0 L10,0 L8,-28 L-8,-28 Z'
    neck = 'M-5,-28 L5,-28 L5,-33 L-5,-33 Z'
    head = ellipse(0, -44, 11, 12)
    arm = ribbon([(-10, -24), (-22, -12), (-28, -4)], 2.5)
    o.append(node(31, ' '.join([torso, neck, head, arm]), '#101010', SEC, 1.5, 640, 320))
    # legs (33, 34): pivot at the hip, hang down with a comically big boot
    def leg(sign):
        return poly([(0, 0), (sign * 18, 6), (sign * 30, 20), (sign * 40, 24), (sign * 36, 30), (sign * 22, 28), (sign * 12, 14), (sign * -2, 4)])
    o.append(node(33, leg(-1), '#101010', SEC, 1.5, 640, 320))
    o.append(node(34, leg(1), '#101010', SEC, 1.5, 640, 320))
    # lasso arm (35): pivot at the shoulder, pointing +x; turned per frame
    o.append(node(35, ribbon([(0, 0), (14, -2), (26, -8)], 3), None, SEC, 1, 640, 320))
    # ---- rig (12): body and faceplate, head-on, around the ram's top (one compound path: a node is one path) ----
    body = ellipse(0, -8, 46, 22)
    ears = 'M-30,-26 L-44,-44 L-32,-40 Z M30,-26 L44,-44 L32,-40 Z'
    hd = poly([(-34, -36), (34, -36), (36, -62), (22, -74), (-22, -74), (-36, -62)])
    muzzle = 'M-14,-40 L14,-40 L10,-30 L-10,-30 Z'
    o.append(node(12, ' '.join([hd, muzzle]), '#161616', SEC, 1.5, 640, 320))
    # the faceplate's facets and vents move with the rig: node 44 rides on the same transform
    o.append(node(44, 'M-22,-74 L-12,-52 L12,-52 L22,-74 L21,-73 L11.5,-51 L-11.5,-51 L-21,-73 Z M-40,-48 L-26,-48 L-26,-47 L-40,-47 Z M26,-48 L40,-48 L40,-47 L26,-47 Z', None, DEC, 1, 640, 320))
    # his face (45): eyes and mouth, on the head
    o.append(node(45, 'M-5,-46.5 L-2,-46.5 L-2,-45.5 L-5,-45.5 Z M2,-46.5 L5,-46.5 L5,-45.5 L2,-45.5 Z M-3,-39.5 L3,-39.5 L3,-38.5 L-3,-38.5 Z', LITE, LITE, 0.5, 640, 320))
    # hat (32): big brim and a tall crown, pivot at the head top
    hat = [(-30, 0), (30, 0), (30, -4), (12, -4), (11, -20), (-11, -20), (-12, -4), (-30, -4)]
    o.append(node(32, poly(hat), '#1A1A1A', LITE, 1.5, 640, 320))
    # ---- horns (13, 14): pivot at the root, pointing +x; mirrored for the left by a negative scale is not
    # allowed (uniform positive scale), so the left horn is authored mirrored ----
    def horn(sign):
        pts = [(0, 0), (10, -3), (22, -8), (32, -16), (38, -26), (40, -34)]
        pts = [(x * sign, y) for x, y in pts]
        up = [(x, y - 4) for x, y in pts]
        down = [(x, y + 4) for x, y in reversed(pts)]
        return poly(up + down)
    o.append(node(13, horn(-1), '#1E1E1E', LITE, 1.5, 640, 320))
    o.append(node(14, horn(1), '#1E1E1E', LITE, 1.5, 640, 320))
    # ---- eye slits (15, 16): a unit-high slit, scaled in y by the frame (uniform scale: so authored at CUT 0 size and the glow does the rest) ----
    o.append(node(15, 'M-10,-2.5 L10,-2.5 L10,2.5 L-10,2.5 Z', LITE, LITE, 0.5, 640, 320))
    o.append(node(16, 'M-10,-2.5 L10,-2.5 L10,2.5 L-10,2.5 Z', LITE, LITE, 0.5, 640, 320))
    # ---- nose ring (17): a ring hanging from its pivot ----
    o.append(node(17, ellipse(0, 9, 9, 8), None, LITE, 2, 640, 320))
    # ---- eye glow (18, 19) ----
    o.append(node(18, 'M-16,-6 L16,-6 L16,6 L-16,6 Z', None, EDGE, 1, 640, 320))
    o.append(node(19, 'M-16,-6 L16,-6 L16,6 L-16,6 Z', None, EDGE, 1, 640, 320))
    # ---- steam puffs (20..23): unit arcs ----
    for i in range(4):
        o.append(node(20 + i, 'M-5,0 C-5,-7 5,-7 5,0 L4,0.4 C4,-5 -4,-5 -4,0.4 Z', None, SEC, 1.5, 640, 320))
    # ---- sparks (24..29): small crosses ----
    for i in range(6):
        o.append(node(24 + i, 'M-3,-0.3 L3,-0.3 L3,0.3 L-3,0.3 Z M-0.3,-3 L0.3,-3 L0.3,3 L-0.3,3 Z', LITE, LITE, 0.5, 640, 320))
    # ---- hose pulse (30): a disc ----
    o.append(node(30, ellipse(0, 0, 3, 3), LITE, LITE, 0.5, 640, 320))

    # ---- the two mutable outlines: 0 the lasso loop, 1 the ripple ----
    for i in range(2):
        pts = [(math.cos(k * 2 * math.pi / 64) * 20, math.sin(k * 2 * math.pi / 64) * 20) for k in range(64)]
        o.append(node(i, poly(pts), None, LITE if i == 0 else DEC, 2, 640, 320))

    # APHX tear slices (36..43): short bright bars, placed per frame
    for i in range(8):
        o.append(node(36 + i, 'M0,-0.6 L18,-0.6 L18,0.6 L0,0.6 Z', LITE, LITE, 0.5, 640, 320))
    for i in range(47, 48):
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


if __name__ == '__main__':
    main()
