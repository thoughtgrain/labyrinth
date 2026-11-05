#!/usr/bin/env python3
"""panel.svg.fragment and nodes.h for toro on the OP-XY: the BLOCKS screen, a field of stacked isometric slabs.

The field is a small model of toro's voice. Every column's height is the voice's waveform at that spot, so turning a
knob reshapes the field the way it reshapes the sound (display.cpp has the mapping). Fold wraps the whole field round
a drum. A note pushes a patch of six slabs up, flashes their tops white and sends a ripple out across the field.

How it's built, and why it fits the device:
  - Each column is three nodes: the left face, the right face and the top. All three are authored at the same
    origin (the top's back corner), so a column moves by one translation and never reshapes.
  - Side faces are fixed 12 px slabs, not floor-length pillars. Columns are drawn back to front (by i + j, then i),
    so each slab's underside is hidden by the columns in front of it. No mutable outlines are needed (paths = 0).
  - Quiet cubes are hollow: black faces with a grey edge (palette 8 and 9). Which cubes those are depends on the
    voice; a fixed scatter of about 28 % (WIRE below) is always hollow, so the field reads as a wireframe and a solid
    at once.
  - Per-column constants the display needs every frame (phases, the detail term, the noise draw, the hollow scatter)
    are worked out here in floating point and written to nodes.h as Q10 tables, so display.cpp does integer maths only.

The design and its contact sheet live in the operators repository (screens/toro/gen_blocks.py, P10 in the patterns).

    python3 gen_panel.py          # writes panel.svg.fragment, nodes.h and the display block in definition.json
Copyright (c) 2026 briannhinton. MIT."""
import json
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = 'toro'
G = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']

NX, NY, S = 10, 7, 15                   # columns across i and j; half-width of a cube's top in px
OX, OY = 240 - (NX - NY) * S / 2, 92    # where column (0, 0)'s base centre sits
PILLAR = 12                             # the side faces: a slab this thick
WIRE_SCATTER = 0.28                     # share of columns that are always hollow
KEY_SPOTS = [(2, 2), (6, 1), (4, 4), (8, 3), (1, 5), (5, 6), (7, 5), (3, 1)]   # where successive notes land
# rest values, matching definition.json's defaults: the fragment is drawn at these
DEFAULTS = {'spread': 0.06, 'fold': 0.18, 'cutoff': 0.55, 'res': 0.24, 'shape': 0.0, 'noise': 0.0, 'drive': 0.0, 'aphx': 0.0}

# palette: (fill, stroke, width). Solid faces stroke in their own fill, so neighbouring faces meet without a seam.
PALETTE = [(G[1], G[1], 0.3), (G[3], G[3], 0.3), (G[4], G[4], 0.3), (G[5], G[5], 0.3), (G[6], G[6], 0.3),
           (G[7], G[7], 0.3), ('#000000', '#000000', 0.3), ('#FFFFFF', '#FFFFFF', 0.3),
           ('#000000', G[4], 0.9), ('#000000', G[6], 0.9)]
PALETTE_NAMES = ['P_RIGHT', 'P_LEFT', 'P_TOP_LO', 'P_TOP_MID', 'P_TOP_HI', 'P_LIT', 'P_BLACK', 'P_FLASH', 'P_WIRE', 'P_WIRE_TOP']

HEADER_FIRST = 0                                       # nodes 0..7: the SDK's two-page header bar
ORDER = sorted([(i, j) for i in range(NX) for j in range(NY)], key=lambda c: (c[0] + c[1], c[0]))   # back to front
FIRST_COL = HEADER_FIRST + 8
NODES = FIRST_COL + 3 * len(ORDER)


def f(v):
    s = f'{v:.3f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(pts):
    return 'M ' + ' L '.join(f'{f(x)} {f(y)}' for x, y in pts) + ' Z'


def base_xy(i, j):
    return OX + (i - j) * S, OY + (i + j) * S * 0.5


# the three faces, relative to the top's back corner
def top_pts():
    return [(0, 0), (S, S * 0.5), (0, S), (-S, S * 0.5)]


def left_pts():
    return [(-S, S * 0.5), (0, S), (0, S + PILLAR), (-S, S * 0.5 + PILLAR)]


def right_pts():
    return [(0, S), (S, S * 0.5), (S, S * 0.5 + PILLAR), (0, S + PILLAR)]


def q10(v):
    return int(round(v * 1024))


def column_tables():
    """per-column constants in ORDER, as display.cpp reads them"""
    rnd = random.Random(4)                              # the noise draw: same sequence as the design's heights()
    noise = {}
    for i in range(NX):
        for j in range(NY):
            noise[(i, j)] = 0.5 * (rnd.random() - 0.5)
    wire = {(i, j) for i in range(NX) for j in range(NY) if random.Random(i * 31 + j * 7 + 3).random() < WIRE_SCATTER}
    rows = []
    for (i, j) in ORDER:
        u, w = i / (NX - 1), j / (NY - 1)
        rows.append({
            'i': i, 'j': j,
            'p1': q10(0.8 * u + 0.4 * w),                                   # the main wave's phase, cycles
            'p2': q10(0.6 * w - 0.5 * u),                                   # the second wave's phase, cycles (× ratio)
            'detail': q10(0.7 * math.sin(2 * math.pi * (3.3 * u + 2.1 * w) + 0.5)),   # the harmonics Cutoff lets in
            'aphx': int(round(math.degrees(i * 2.1 + j * 1.3) * 10)) % 3600,  # each column's own drift angle, tenths
            'noise': q10(noise[(i, j)]),
            'wire': 1 if (i, j) in wire else 0,
        })
    return rows


def rest_pose():
    """where each column sits in the fragment: flat field, the defaults' heights, no note"""
    v = DEFAULTS
    out = {}
    rnd = random.Random(4)
    for i in range(NX):
        for j in range(NY):
            u, w = i / (NX - 1), j / (NY - 1)
            ph = (0.8 * u + 0.4 * w) % 1
            osc1 = (2 * ph - 1) * (1 - v['shape']) + (1 - 4 * abs(ph - 0.5)) * v['shape']
            osc2 = 0.6 * math.sin(2 * math.pi * (1 + 0.5 * v['spread']) * (0.6 * w - 0.5 * u) + 1)
            raw = 0.45 * (osc1 + osc2) + v['cutoff'] * 0.7 * math.sin(2 * math.pi * (3.3 * u + 2.1 * w) + 0.5)
            s = (1 + 7 * v['fold']) * raw
            while s > 1 or s < -1:
                s = 2 - s if s > 1 else -2 - s
            s += v['noise'] * 0.5 * (rnd.random() - 0.5)
            h = 8 + 32 * (max(-1.0, min(1.0, s)) + 1) / 2
            bx, by = base_xy(i, j)
            out[(i, j)] = (round(bx), round(by - round(h)), h)
    return out


def header_fragment():
    o = ['<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
         '<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000" stroke="none"/>']
    for i in range(8):
        o.append(f'<path id="{PREFIX}_node_{HEADER_FIRST + i}" transform="matrix(1 0 0 1 {60 * i} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" fill="{G[i]}" stroke="{G[i]}" stroke-width="0.5"/>')
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
    tables = column_tables()
    rest = rest_pose()
    o = [f'<g id="{PREFIX}_panel">'] + header_fragment()
    for k, (i, j) in enumerate(ORDER):
        x, y, h = rest[(i, j)]
        hollow = tables[k]['wire'] or h < 8 + 0.35 * 32
        faces = [(left_pts(), 8 if hollow else 1), (right_pts(), 8 if hollow else 0), (top_pts(), 9 if hollow else (4 if h > 30 else 3 if h > 18 else 2))]
        for n, (pts, p) in enumerate(faces):
            fill, stroke, width = PALETTE[p]
            o.append(f'<path id="{PREFIX}_node_{FIRST_COL + 3 * k + n}" transform="matrix(1 0 0 1 {x} {y})" d="{poly(pts)}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    for i, (fill, stroke, width) in enumerate(PALETTE):
        o.append(f'<path id="{PREFIX}_palette_{i}" transform="matrix(1 0 0 1 640 320)" d="M0,0 L4,0 L4,4 L0,4 Z" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    o.append('</g>')
    (HERE / 'panel.svg.fragment').write_text('\n'.join(o) + '\n', encoding='utf-8')

    def row(name, key, ctype='short'):
        return f'const {ctype} {name}[COL_N] = {{ ' + ', '.join(str(t[key]) for t in tables) + ' };\n'

    h = ('#pragma once\n// toro for the OP-XY: node indices and the field\'s per-column tables, written by gen_panel.py.\n'
         '// Edit the generator, not this file.\nnamespace toro {\n')
    h += f'enum {{ N_HDR = {HEADER_FIRST}, N_COL = {FIRST_COL}, COL_N = {len(ORDER)}, N_TOTAL = {NODES} }};\n'
    h += 'enum { ' + ', '.join(f'{n} = {k}' for k, n in enumerate(PALETTE_NAMES)) + f', PALETTE_N = {len(PALETTE)} }};\n'
    h += f'const int NX = {NX}, NY = {NY}, CUBE = {S};\n'
    h += f'const int ORIGIN_X2 = {round(2 * OX)}, ORIGIN_Y2 = {round(2 * OY)};    // column (0, 0)\'s base centre, half pixels\n'
    h += f'const int KEY_SPOT_N = {len(KEY_SPOTS)};\n'
    h += 'const unsigned char KEY_I[KEY_SPOT_N] = { ' + ', '.join(str(a) for a, _ in KEY_SPOTS) + ' };\n'
    h += 'const unsigned char KEY_J[KEY_SPOT_N] = { ' + ', '.join(str(b) for _, b in KEY_SPOTS) + ' };\n'
    h += '// columns in drawing order (back to front); column k is nodes N_COL + 3k (left face, right face, top)\n'
    h += row('COL_I', 'i', 'unsigned char') + row('COL_J', 'j', 'unsigned char')
    h += '// Q10: the main wave\'s phase, the second wave\'s phase, the detail term, the noise draw\n'
    h += row('COL_P1', 'p1') + row('COL_P2', 'p2') + row('COL_DETAIL', 'detail') + row('COL_NOISE', 'noise')
    h += '// tenths of a degree: each column\'s own APHX drift angle; 1: always hollow\n'
    h += row('COL_APHX', 'aphx') + row('COL_WIRE', 'wire', 'unsigned char')
    h += '}\n'
    (HERE / 'nodes.h').write_text(h, encoding='utf-8')

    definition = json.loads((HERE / 'definition.json').read_text(encoding='utf-8'))
    d = definition['resources']['display']
    d.update({'stateBytes': 52, 'paths': 0, 'points': 0, 'nodes': NODES, 'texts': 16, 'palette': len(PALETTE)})   # stateBytes: sizeof(DisplayState)
    (HERE / 'definition.json').write_text(json.dumps(definition, indent=2) + '\n', encoding='utf-8')
    print(f'{NODES} nodes, {len(ORDER)} columns, {len(PALETTE)} palette entries')


if __name__ == '__main__':
    main()
