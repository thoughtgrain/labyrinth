const fs = require('fs'), D = __dirname + '/';
const src = ['core.js', 'layouts.js', 'engine.js', 'thread.js', 'bull.js', 'icons.js'].map(f => fs.readFileSync(D + f, 'utf8')).join('\n');
const api = new Function(src + '\nreturn {ML, TKL, ICON_LAYERS, iconPaths, LAYOUTS, XS, SZ, meikyuInit, meikyuStep, meikyuTick, threadDraw, toroScene, meikyuLen};')();
const COLS = ['#698eff', '#00ed95', '#ffffff', '#ff423f'];
function svg(L, g, layout, N, V, U, vb) {
  const Lo = api.LAYOUTS[layout];
  let o = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${vb || '0 0 320 160'}" width="640" height="320"><rect x="-10" y="-10" width="340" height="180" fill="#000"/>`;
  const ic = api.iconPaths(layout, N); for (const [key, f, s, w] of api.ICON_LAYERS) o += `<path d="${ic[key]}" fill="${f}" stroke="${s}" stroke-width="${w}" stroke-linecap="round" stroke-linejoin="round"/>`;
  for (const [key, f, s, w] of L) o += `<path d="${g[key]}" fill="${f}" stroke="${s}" stroke-width="${w}" stroke-linecap="round" stroke-linejoin="round"/>`;
  (Lo.box || []).forEach(b => o += `<rect x="${b[0]}" y="${b[1]}" width="${b[2]}" height="${b[3]}" rx="4" fill="none" stroke="#625d77" stroke-width="1.5"/>`);
  Lo.slots.forEach((s, i) => { o += `<text x="${s.n[0]}" y="${s.n[1]}" text-anchor="${s.n[2]}" font-family="DejaVu Sans Mono" font-size="${api.XS}" fill="${COLS[i]}">${N[i]}</text><text x="${s.v[0]}" y="${s.v[1]}" text-anchor="${s.v[2]}" font-family="DejaVu Sans Mono" fill="${COLS[i]}"><tspan font-size="${api.SZ}">${V[i]}</tspan><tspan font-size="${api.XS}">${U[i]}</tspan></text>`; });
  return o + '</svg>';
}
const [out, which, kj, fr, vb, pg] = process.argv.slice(2), k = JSON.parse(kj);
if (which === 'm') { const st = api.meikyuInit(7); for (let f = 0; f < Number(fr); f++) { if (k.running && f % 3 === 0) api.meikyuStep(st, k); api.meikyuTick(st); }
  fs.writeFileSync(out, pg === 'shift' ? svg(api.ML, api.threadDraw(k, st), 'MK', ['RATE', 'GATE', 'SCL', 'HOLD'], ['1/16', '50', 'MIN', 'ON'], ['', '%', '', ''], vb) : svg(api.ML, api.threadDraw(k, st), 'MK', ['CORR', 'LEN', 'DENS', 'RNG'], [String(Math.round(k.corr * 100)), String(api.meikyuLen(k)), String(Math.round(k.dens * 100)), String(Math.round(k.rng * 15))], ['%', '', '%', ''], vb)); }
else fs.writeFileSync(out, svg(api.TKL, api.toroScene('rider', { ...k, t: Number(fr), gate: true, n: 1 }), 'TK', k.page2 ? ['SHAP', 'NOIS', 'DRIV', 'RES'] : ['SPRD', 'FOLD', 'CUT', 'RES'], [k.sprd, k.fold, k.cut, k.res].map(v => String(Math.round(v * 99))), ['', '', '', ''], vb));
