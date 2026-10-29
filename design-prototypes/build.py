import json, subprocess
H = '/tmp/claude-0/-home-claude/ce25d2e9-4e96-582b-ac98-fd25325eff4a/scratchpad/hero4/'
P = '/tmp/claude-0/-home-claude/ce25d2e9-4e96-582b-ac98-fd25325eff4a/scratchpad/canvas/project/'
LIB = ['core.js', 'layouts.js', 'engine.js', 'thread.js', 'bull.js', 'icons.js']
def js(expr):
    return json.loads(subprocess.run(['node', '-e', ''.join(open(H + f).read() + '\n' for f in LIB) + f';console.log(JSON.stringify({expr}))'], capture_output=True, text=True, check=True).stdout)
LAY = js('LAYOUTS'); XS, SZ = js('[XS, SZ]')
COLS = ['#698eff', '#00ed95', '#ffffff', '#ff423f']; FONT = "'JetBrains Mono', ui-monospace, monospace"
BTN = 'min-height: 44px; padding: 0 16px; border-radius: 8px; border: 1.5px solid #b9b9c9; font-family: inherit; font-size: 14px; cursor: pointer'
def page(fname, title, h1, desc, aria, layers, layout, comp):
    L = js(layers) + js('ICON_LAYERS'); Lo = LAY[layout]
    paths = '\n'.join(f'<path d="{{{{g.{k}}}}}" fill="{f}" stroke="{s}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"></path>' for k, f, s, w in L)
    boxes = '\n'.join(f'<rect x="{b[0]}" y="{b[1]}" width="{b[2]}" height="{b[3]}" rx="4" fill="none" stroke="#625d77" stroke-width="1.5"></rect>' for b in Lo.get('box', []))
    texts = '\n'.join(f'<text x="{s["n"][0]}" y="{s["n"][1]}" text-anchor="{s["n"][2]}" font-family="{FONT}" font-size="{XS}" fill="{COLS[i]}">{{{{n{i}}}}}</text>\n'
                      f'<text x="{s["v"][0]}" y="{s["v"][1]}" text-anchor="{s["v"][2]}" font-family="{FONT}" fill="{COLS[i]}"><tspan font-size="{SZ}">{{{{v{i}}}}}</tspan><tspan font-size="{XS}">{{{{u{i}}}}}</tspan></text>' for i, s in enumerate(Lo['slots']))
    src = '\n'.join(open(H + f).read() for f in LIB) + '\n' + open(H + comp).read()
    html = f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500&amp;display=swap">
<style>
body{{margin:0}}
</style>
</helmet>
<div style="width: 1040px; height: 600px; box-sizing: border-box; padding: 28px 32px; background: #0e0e10; font-family: 'JetBrains Mono', ui-monospace, monospace; color: #b9b9c9; display: flex; flex-direction: column; gap: 14px">
<div style="display: flex; align-items: baseline; gap: 16px">
<h1 style="margin: 0; font-size: 24px; font-weight: 500; color: #ffffff; white-space: nowrap">{h1}</h1>
<p style="margin: 0; font-size: 14px; color: #b9b9c9">{desc}</p>
</div>
<div style="display: flex; gap: 32px; align-items: flex-start">
<div style="display: flex; flex-direction: column; gap: 16px; width: 640px">
<svg viewBox="0 0 320 160" width="640" height="320" role="img" aria-label="{aria}" style="display: block; border-radius: 6px; background: #000000">
{paths}
{boxes}
{texts}
</svg>
<div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px 24px">
<sc-for list="{{{{legend}}}}" as="l" hint-placeholder-count="4">
<p style="margin: 0; font-size: 12px; line-height: 1.45; color: #b9b9c9"><span style="color: {{{{l.color}}}}; font-weight: 500">{{{{l.name}}}}</span> {{{{l.desc}}}}</p>
</sc-for>
</div>
</div>
<div style="display: flex; flex-direction: column; gap: 16px; width: 300px">
<sc-for list="{{{{dials}}}}" as="d" hint-placeholder-count="4">
<label style="display: flex; flex-direction: column; gap: 6px; font-size: 14px">
<span style="display: flex; justify-content: space-between"><span style="color: {{{{d.color}}}}">{{{{d.name}}}}</span><span style="color: #ffffff">{{{{d.show}}}}</span></span>
<input type="range" min="0" max="100" step="1" value="{{{{d.v}}}}" onChange="{{{{d.set}}}}" style="width: 100%; accent-color: {{{{d.color}}}}">
</label>
</sc-for>
<button type="button" onClick="{{{{toggle}}}}" aria-pressed="{{{{pressed}}}}" style="{BTN}; background: {{{{toggleBg}}}}; color: {{{{toggleFg}}}}">{{{{toggleLabel}}}}</button>
<button type="button" onClick="{{{{toggle2}}}}" aria-pressed="{{{{pressed2}}}}" style="{BTN}; background: {{{{toggle2Bg}}}}; color: {{{{toggle2Fg}}}}">{{{{toggle2Label}}}}</button>
<p role="status" style="margin: 0; font-size: 12px; line-height: 1.45; color: #b9b9c9">{{{{state}}}}</p>
</div>
</div>
</div>
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":1040,"height":600}}}}'>
{src}
</script>
</body>
</html>
'''
    open(P + fname, 'w').write(html)
page('Final-meikyu-thread.dc.html', 'Meikyu thread', 'Meikyu · Thread', "Ariadne's thread runs the corridors toward the bull. Hold a key to play; HOLD latches it.", 'OP-1 screen: meikyu, a thread through a round labyrinth', 'ML', 'MK', 'comp_m.js')
page('Final-toro-rider.dc.html', 'Toro rider', 'Toro · Rider', 'A robot bull with a cowboy aboard; each readout sits by the part it drives. Motion follows each note.', 'OP-1 screen: toro, robot bull with a cowboy rider', 'TKL', 'TK', 'comp_t.js')
print('built')
