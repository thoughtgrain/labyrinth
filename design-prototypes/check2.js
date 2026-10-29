// Fine-grained rule check for bespoke placements: samples every artwork path into points (with stroke width)
// and requires 2 units between any ink and any text box, artwork inside the safe area, text at or above y=140.
const fs = require('fs'), D = __dirname + '/';
const src = ['core.js', 'layouts.js', 'engine.js', 'thread.js', 'bull.js', 'icons.js'].map(f => fs.readFileSync(D + f, 'utf8')).join('\n');
const api = new Function(src + '\nreturn {ML, TKL, TSC, ICON_LAYERS, iconPaths, LAYOUTS, slotBoxes, meikyuInit, meikyuStep, meikyuTick, threadDraw, toroScene};')();
function sample(d) {
  const t = d.match(/[MLCQZ]|-?[\d.]+/g) || [], pts = []; let i = 0, c = '', x = 0, y = 0, sx = 0, sy = 0;
  const num = () => Number(t[i++]);
  while (i < t.length) {
    if (/[MLCQZ]/.test(t[i])) c = t[i++];
    if (c === 'M') { x = sx = num(); y = sy = num(); pts.push([x, y]); c = 'L'; }
    else if (c === 'L') { const nx = num(), ny = num(); for (let j = 1; j <= 6; j++) pts.push([x + (nx - x) * j / 6, y + (ny - y) * j / 6]); x = nx; y = ny; }
    else if (c === 'C') { const a = [num(), num(), num(), num(), num(), num()]; for (let j = 1; j <= 10; j++) { const s = j / 10, u = 1 - s;
        pts.push([u * u * u * x + 3 * u * u * s * a[0] + 3 * u * s * s * a[2] + s * s * s * a[4], u * u * u * y + 3 * u * u * s * a[1] + 3 * u * s * s * a[3] + s * s * s * a[5]]); } x = a[4]; y = a[5]; }
    else if (c === 'Q') { const a = [num(), num(), num(), num()]; for (let j = 1; j <= 8; j++) { const s = j / 8, u = 1 - s; pts.push([u * u * x + 2 * u * s * a[0] + s * s * a[2], u * u * y + 2 * u * s * a[1] + s * s * a[3]]); } x = a[2]; y = a[3]; }
    else if (c === 'Z') { pts.push([sx, sy]); x = sx; y = sy; c = ''; }
    else i++;
  }
  return pts;
}
function check(name, L, layout, frames, names) {
  const boxes = names.flatMap(([n, v, u]) => api.slotBoxes(layout, n, v, u).flat()), errs = new Set();
  api.LAYOUTS[layout].slots.forEach(s => boxes.push([s.ic[0], s.ic[1] - 11.5, s.ic[0] + 10, s.ic[1] - 1.5]));   // icon boxes count as readout ink
  names.forEach(([n, v, u]) => {                          // within one page: icon to name gap, and no part of one readout near another
    const parts = api.slotBoxes(layout, n, v, u).map((p, i) => { const s = api.LAYOUTS[layout].slots[i]; return [[s.ic[0], s.ic[1] - 11.5, s.ic[0] + 10, s.ic[1] - 1.5], ...p]; });
    parts.forEach((pi, i) => { const [ic, nm] = pi, gap = Math.max(nm[0] - ic[2], ic[0] - nm[2]); if (gap < 3.9) errs.add('icon too close to its name ' + n[i] + ' ' + gap.toFixed(1));
      parts.forEach((pj, j) => { if (j <= i) return; pi.forEach(a => pj.forEach(b => { if (a[0] < b[2] + 8 && b[0] < a[2] + 8 && a[1] < b[3] + 8 && b[1] < a[3] + 8) errs.add('readouts ' + n[i] + '/' + n[j] + ' closer than 8'); })); }); });
  });
  let near = 1e9;
  boxes.forEach(b => { if (b[0] < 8 || b[2] > 312 || b[1] < 8 || b[3] > 140) errs.add('text outside safe area'); });
  for (const g of frames) for (const [key, f, s, w] of L) {
    if (g[key] === 'M 0 0') continue;
    const hw = s === 'none' ? 0 : w / 2;
    for (const [x, y] of sample(g[key])) {
      if (x - hw < 8 || x + hw > 312 || y - hw < 8 || y + hw > 144) errs.add(`${key} outside safe area at ${x.toFixed(1)},${y.toFixed(1)}`);
      for (const b of boxes) { const dx = Math.max(b[0] - x, 0, x - b[2]), dy = Math.max(b[1] - y, 0, y - b[3]), dd = Math.hypot(dx, dy) - hw;
        near = Math.min(near, dd); if (dd < 2) errs.add(`${key} within 2 of text box ${b.map(v => v.toFixed(0))}`); }
    }
  }
  console.log((errs.size ? 'FAIL ' : 'ok   ') + name + `  closest ink-to-text gap ${near.toFixed(1)}`, errs.size ? '\n   ' + [...errs].slice(0, 8).join('\n   ') : '');
}
{ const fr = [];
  for (const corr of [0, 1]) for (const len of [0, 0.5, 1]) for (const rngv of [0, 1]) for (const hold of [false, true]) {
    const k = { corr, len, dens: 1, rng: rngv, hold, running: true, stepFrames: 3, gateFrames: 3 }, st = api.meikyuInit(3);
    for (let f = 0; f < 100; f++) { if (f % 3 === 0) api.meikyuStep(st, k); api.meikyuTick(st); if (f % 2 === 0) fr.push(api.threadDraw(k, st)); } }
  check('meikyu thread', api.ML, 'MK', fr, [[['CORR', 'LEN', 'DENS', 'RNG'], ['100', '16', '100', '15'], ['%', '', '%', '']], [['RATE', 'GATE', 'SCL', 'HOLD'], ['1/16', '100', 'MIN', 'OFF'], ['', '%', '', '']]]); }
{ const fr = [];
  for (const sprd of [0, 1]) for (const fold of [0, 1]) for (const res of [0, 1]) for (const page2 of [false, true]) for (let t = 0; t < 0.6; t += 0.03) for (const n of [0, 1])
    fr.push(api.toroScene('rider', { sprd, fold, cut: 1, res, shape: page2 ? 1 : 0, noise: 1, drive: 1, page2, t, gate: t < 0.3, n }));
  check('toro rider', api.TKL, 'TK', fr, [[['SPRD', 'FOLD', 'CUT', 'RES'], ['99', '99', '99', '99'], ['', '', '', '']], [['SHAP', 'NOIS', 'DRIV', 'RES'], ['99', '99', '99', '99'], ['', '', '', '']]]); }
