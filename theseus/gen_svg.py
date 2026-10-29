#!/usr/bin/env python3
"""theseus.svg: the PHALANX screen's art, all static: the kit's shells, heads and stands, and the drummer's cuirass.
The helmet is drawn by code (theseus_screen.c), because it turns between a profile and a front view; so are the
cymbals, the crest, the eyes, the arms, sticks, beater, ripples and readouts. Numbers match theseus_screen.c (TH_* and the DRUM table).
Native subset: one filled contour per path, cubic curves only, <= 3 decimals.
    python3 gen_svg.py > theseus.svg
Copyright (C) 2026 briannhinton. GPL-3.0-or-later with the OP-1 firmware permission (see kennel's LICENSE and NOTICE)."""
K = 0.5523
SHELL, HEAD, EDGE, DEC, HELM = '#1B1A22', '#2A2735', '#B9B9C9', '#625D77', '#47435A'


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def ellipse(cx, cy, rx, ry):
    return (f'M{f(cx + rx)} {f(cy)} C{f(cx + rx)} {f(cy + K * ry)} {f(cx + K * rx)} {f(cy + ry)} {f(cx)} {f(cy + ry)} '
            f'C{f(cx - K * rx)} {f(cy + ry)} {f(cx - rx)} {f(cy + K * ry)} {f(cx - rx)} {f(cy)} '
            f'C{f(cx - rx)} {f(cy - K * ry)} {f(cx - K * rx)} {f(cy - ry)} {f(cx)} {f(cy - ry)} '
            f'C{f(cx + K * rx)} {f(cy - ry)} {f(cx + rx)} {f(cy - K * ry)} {f(cx + rx)} {f(cy)} Z')


def shell(cx, cy, rx, ry, h):
    """a drum seen from slightly above: the side wall down to the bottom rim (the head goes on top separately)"""
    b = cy + h
    return (f'M{f(cx - rx)} {f(cy)} L{f(cx - rx)} {f(b)} C{f(cx - rx)} {f(b + K * ry)} {f(cx - K * rx)} {f(b + ry)} {f(cx)} {f(b + ry)} '
            f'C{f(cx + K * rx)} {f(b + ry)} {f(cx + rx)} {f(b + K * ry)} {f(cx + rx)} {f(b)} L{f(cx + rx)} {f(cy)} '
            f'C{f(cx + rx)} {f(cy + K * ry)} {f(cx + K * rx)} {f(cy + ry)} {f(cx)} {f(cy + ry)} '
            f'C{f(cx - K * rx)} {f(cy + ry)} {f(cx - rx)} {f(cy + K * ry)} {f(cx - rx)} {f(cy)} Z')


def line(pts, col=DEC, w=1.5):
    return f'<path fill="none" stroke="{col}" stroke-width="{w}" d="M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in pts) + '"/>'


def main():
    o = ['<?xml version="1.0" encoding="utf-8"?>',
         '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="320px" height="160px" viewBox="0 0 320 160">']
    # stands first, behind everything: hi-hat (pole, tripod, pedal), cymbal, snare, floor-tom legs, kick spurs
    o.append(line([(94, 80), (94, 132)]))
    o.append(line([(87, 140), (94, 131), (101, 140)]))
    o.append(line([(232, 69), (234, 131)]))
    o.append(line([(227, 140), (234, 130), (241, 140)]))
    o.append(line([(122, 107), (122, 131)]))
    o.append(line([(115, 140), (122, 130), (129, 140)]))
    o.append(line([(197, 120), (195, 140)]))
    o.append(line([(217, 120), (219, 140)]))
    o.append(line([(143, 126), (136, 140)]))
    o.append(line([(177, 126), (184, 140)]))
    # the drummer's cuirass, behind the kit (the helmet is drawn on it by code)
    o.append(f'<path id="cuirass" fill="{SHELL}" stroke="{DEC}" stroke-width="1.5" d="M141 95 C143 89 150 87 158 87 '
             f'C166 87 173 89 175 95 L170 106 L146 106 Z"/>')
    # snare, floor tom, rack tom (with its mount), kick
    for cx, cy, rx, ry, h in ((122, 98, 14, 4, 8), (207, 108, 13, 4, 16), (190, 88, 11, 3.5, 8)):
        o.append(f'<path fill="{SHELL}" stroke="{EDGE}" stroke-width="1.5" d="{shell(cx, cy, rx, ry, h)}"/>')
        o.append(f'<path fill="{HEAD}" stroke="{EDGE}" stroke-width="1.5" d="{ellipse(cx, cy, rx, ry)}"/>')
    o.append(line([(176, 99), (186, 96)], EDGE))
    o.append(f'<path id="kick" fill="{SHELL}" stroke="{EDGE}" stroke-width="2" d="{ellipse(160, 114, 22, 22)}"/>')
    o.append(f'<path fill="none" stroke="{DEC}" stroke-width="1" d="{ellipse(160, 114, 18.5, 18.5)}"/>')
    o.append('</svg>')
    print('\n'.join(o))


if __name__ == '__main__':
    main()
