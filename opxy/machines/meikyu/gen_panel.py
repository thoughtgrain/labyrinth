#!/usr/bin/env python3
"""panel.svg.fragment for the meikyu player page on the OP-XY (480 x 220).

A player page is static artwork plus the SDK's bindings: 24 dots the player lights (meikyu_dot_0..23), one text
line (meikyu_text), and the output keyboard (meikyu_key_0..127 and meikyu_played_0..127) the SDK lays out itself in
the lower row at y=174. The OP-1's labyrinth comes over as the drawing: the sixteen register steps sit on a spiral
as diamonds (dots 0..15, lit when a step plays at the current density), and the eight tones of the last phrase run
along a track on the right (dots 16..23, lit when they sounded). Greyscale.

    python3 gen_panel.py > panel.svg.fragment
Copyright (c) 2026 briannhinton. MIT."""
import math

EDGE, SEC, DEC, LITE, DARK = '#CBCBCB', '#9A9A9A', '#5A5A5A', '#F0F0F0', '#2A2A2A'
CX, CY = 128, 96                      # the labyrinth's centre
R0, R1 = 13, 58                       # the spiral's inner and outer radius


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(pts, close=True):
    return 'M' + ' L'.join(f'{f(x)},{f(y)}' for x, y in pts) + (' Z' if close else '')


def spiral_point(u):
    """u 0..1 along the spiral, two turns from the outside in"""
    a = -math.pi / 2 + u * 2.0 * 2 * math.pi
    r = R1 - (R1 - R0) * u
    return CX + math.cos(a) * r, CY + math.sin(a) * r


def dot(identity, x, y, radius, fill):
    return f'<circle id="{identity}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" cx="0" cy="0" r="{radius}" fill="{fill}" stroke="{fill}" stroke-width="0.5"/>'


def diamond(identity, x, y, s, fill):
    return (f'<path id="{identity}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" d="M0,{f(-s)} L{f(s)},0 L0,{f(s)} L{f(-s)},0 Z" '
            f'fill="{fill}" stroke="{fill}" stroke-width="0.5"/>')


def text(x, y, value, size=11, fill=SEC, anchor='start'):
    return f'<text transform="matrix(1 0 0 1 {f(x)} {f(y)})" fill="{fill}" font-family="UniversNextPro-Light" font-size="{size}" text-anchor="{anchor}">{value}</text>'


def main():
    o = ['<g id="meikyu_panel">', '<rect x="0" y="20" width="480" height="200" fill="#000000"/>']
    # the labyrinth: the spiral corridor as two walls (the spiral and the same spiral a half-turn in), drawn in runs
    # of at most 64 points each, the device's limit per path; a few wall stubs across the corridor, as on the OP-1
    def runs(points, fill='none', stroke=DEC, w=1.2):
        for k in range(0, len(points) - 1, 60):
            seg = points[k:k + 61]
            o.append('<path d="M' + ' L'.join(f'{f(x)},{f(y)}' for x, y in seg) + f'" fill="{fill}" stroke="{stroke}" stroke-width="{w}"/>')
    runs([spiral_point(k / 240) for k in range(241)], stroke=DEC, w=1.4)
    for u in (0.19, 0.44, 0.69):
        x, y = spiral_point(u)
        a = math.atan2(y - CY, x - CX)
        dx, dy = math.cos(a) * 7.5, math.sin(a) * 7.5
        o.append(f'<path d="{poly([(x - dx - math.sin(a) * 0.7, y - dy + math.cos(a) * 0.7), (x + dx - math.sin(a) * 0.7, y + dy + math.cos(a) * 0.7), (x + dx + math.sin(a) * 0.7, y + dy - math.cos(a) * 0.7), (x - dx + math.sin(a) * 0.7, y - dy - math.cos(a) * 0.7)])}" fill="{DEC}" stroke="{DEC}" stroke-width="0.5"/>')
    # the centre: the sequencer's eye
    o.append(f'<circle cx="{CX}" cy="{CY}" r="5" fill="none" stroke="{SEC}" stroke-width="1.2"/>')
    o.append(f'<circle cx="{CX}" cy="{CY}" r="1.6" fill="{SEC}" stroke="{SEC}" stroke-width="0.5"/>')
    # the sixteen steps as diamonds along the spiral: the A half (0..7) outside, the B half (8..15) inside; step 0 at the top
    for i in range(16):
        x, y = spiral_point(0.015 + i * 0.955 / 16)
        o.append(f'<path d="M{f(x)},{f(y - 6.5)} L{f(x + 6.5)},{f(y)} L{f(x)},{f(y + 6.5)} L{f(x - 6.5)},{f(y)} Z" fill="#000000" stroke="{SEC}" stroke-width="1.2"/>')
        o.append(diamond(f'meikyu_dot_{i}', x, y, 4, LITE))
    xa, ya = spiral_point(0.02); xb, yb = spiral_point(0.02 + 8 * 0.96 / 16)
    o.append(text(xa + 8, ya - 4, 'A', 10, DEC))
    o.append(text(xb - 14, yb + 3, 'B', 10, DEC))

    # the phrase track on the right: eight cells, the last phrase's tones light them in order
    x0, y0, gap = 228, 96, 27
    o.append(f'<path d="M{x0 - 8},{y0} L{x0 + 7 * gap + 8},{y0}" fill="none" stroke="{DEC}" stroke-width="1.2"/>')
    for k in range(8):
        x = x0 + k * gap
        o.append(f'<rect x="{f(x - 8)}" y="{f(y0 - 8)}" width="16" height="16" fill="#000000" stroke="{SEC}" stroke-width="1.2"/>')
        o.append(dot(f'meikyu_dot_{16 + k}', x, y0, 4.5, LITE))
        o.append(text(x, y0 + 22, str(k + 1), 9, DEC, 'middle'))
    o.append(text(x0 - 8, y0 - 18, 'phrase', 11, SEC))
    o.append(text(x0 + 7 * gap + 8, y0 - 18, 'one tone per step', 9, DEC, 'end'))
    # the text line: the last root, which half the phrase began in, and the count
    o.append(f'<text id="meikyu_text" transform="matrix(1 0 0 1 240 157)" fill="#F2F2F2" font-family="UniversNextPro-Light" font-size="18" text-anchor="middle">meikyu</text>')

    # the output keyboard the SDK lays out: every MIDI pitch has a key and an activity dot, white keys before black
    def black(note): return note % 12 in (1, 3, 6, 8, 10)
    for is_black in (False, True):
        for note in range(128):
            if black(note) != is_black:
                continue
            w, h = (6, 27) if is_black else (10, 44)
            fill = '#000000' if is_black else '#F0F0F0'
            o.append(f'<path id="meikyu_key_{note}" transform="matrix(1 0 0 1 1024 174)" d="M0 0 H{w} V{h} H0 Z" fill="{fill}" stroke="#000000" stroke-width="0.5"/>')
    for note in range(128):
        fill = '#F0F0F0' if black(note) else '#000000'
        o.append(dot(f'meikyu_played_{note}', 1024, 0, 2.5, fill))
    o.append('</g>')
    print('\n'.join(o))


if __name__ == '__main__':
    main()
