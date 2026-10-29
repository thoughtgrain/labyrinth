// Hero core: painter's-order layers instead of one path per paint, so solid shapes can sit in front of each other.
// Each layer is one <path>: [key, fill, stroke, width]. A figure is drawn by putting shapes into its layers.
function n(v) { return Math.round(v * 100) / 100; }
function grp(L) { const g = {}; L.forEach(l => g[l[0]] = []); return g; }
function out(g, L) { const o = {}; L.forEach(l => o[l[0]] = g[l[0]].join(' ') || 'M 0 0'); return o; }
function d(shape) { return shape.map(c => c[0] === 'Z' ? 'Z' : c[0] + ' ' + c.slice(1).map(n).join(' ')).join(' '); }
function put(g, k, shape) { if (!g[k]) throw new Error('no layer ' + k); g[k].push(typeof shape === 'string' ? shape : d(shape)); }
function xf(shape, t) {                                   // t: {x, y, a, s, px, py}
  const c = Math.cos(t.a || 0), si = Math.sin(t.a || 0), s = t.s || 1, px = t.px || 0, py = t.py || 0;
  const p = (x, y) => { const X = (x - px) * s, Y = (y - py) * s; return [n(px + X * c - Y * si + (t.x || 0)), n(py + X * si + Y * c + (t.y || 0))]; };
  return shape.map(cmd => { if (cmd[0] === 'Z') return cmd; const r = [cmd[0]]; for (let i = 1; i < cmd.length; i += 2) r.push(...p(cmd[i], cmd[i + 1])); return r; });
}
function mirror(shape, cx) { return shape.map(c => c[0] === 'Z' ? c : [c[0], ...c.slice(1).map((v, i) => i % 2 ? v : 2 * cx - v)]); }
function line(x1, y1, x2, y2) { return [['M', x1, y1], ['L', x2, y2]]; }
function poly(pts, close) { const s = pts.map((p, i) => [i ? 'L' : 'M', p[0], p[1]]); if (close) s.push(['Z']); return s; }
function circle(cx, cy, r) {
  const k = 0.5523 * r;
  return [['M', cx + r, cy], ['C', cx + r, cy + k, cx + k, cy + r, cx, cy + r], ['C', cx - k, cy + r, cx - r, cy + k, cx - r, cy],
          ['C', cx - r, cy - k, cx - k, cy - r, cx, cy - r], ['C', cx + k, cy - r, cx + r, cy - k, cx + r, cy], ['Z']];
}
function ellipse(cx, cy, rx, ry) { return circle(0, 0, 1).map(c => c[0] === 'Z' ? c : [c[0], ...c.slice(1).map((v, i) => i % 2 ? cy + v * ry : cx + v * rx)]); }
function arcPts(cx, cy, rx, ry, a0, a1, steps) { const p = []; for (let i = 0; i <= steps; i++) { const a = a0 + (a1 - a0) * i / steps; p.push([cx + rx * Math.cos(a), cy + ry * Math.sin(a)]); } return p; }
function rr(x, y, w, h, r) {
  return [['M', x + r, y], ['L', x + w - r, y], ['C', x + w, y, x + w, y, x + w, y + r], ['L', x + w, y + h - r], ['C', x + w, y + h, x + w, y + h, x + w - r, y + h],
          ['L', x + r, y + h], ['C', x, y + h, x, y + h, x, y + h - r], ['L', x, y + r], ['C', x, y, x, y, x + r, y], ['Z']];
}
function rng(seed) { let s = seed >>> 0 || 1; return () => { s ^= s << 13; s >>>= 0; s ^= s >> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; }
function bez(p0, p1, p2, p3, steps) {                     // sample a cubic into points (not including p0)
  const o = [];
  for (let i = 1; i <= steps; i++) { const t = i / steps, u = 1 - t;
    o.push([u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0], u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]]); }
  return o;
}
// A polyline measured by arc length, so things can travel along it and be cut out of it.
function track(pts) { const s = [0]; for (let i = 1; i < pts.length; i++) s.push(s[i - 1] + Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1])); return { pts, s, len: s[s.length - 1] }; }
function at(tr, d) {                                      // point and unit tangent at distance d
  d = Math.max(0, Math.min(tr.len, d));
  let i = 1; while (i < tr.s.length - 1 && tr.s[i] < d) i++;
  const a = tr.pts[i - 1], b = tr.pts[i], seg = (tr.s[i] - tr.s[i - 1]) || 1, t = (d - tr.s[i - 1]) / seg;
  const tx = (b[0] - a[0]) / seg, ty = (b[1] - a[1]) / seg;
  return { x: a[0] + (b[0] - a[0]) * t, y: a[1] + (b[1] - a[1]) * t, tx, ty };
}
// Tube outline around a spine: one closed contour, left edge out and right edge back, round caps.
// spine: [{x,y,tx,ty,w}] from tail to head, w = half width.
function tube(spine, capHead, capTail) {
  const L = [], R = [];
  spine.forEach(p => { const nx = -p.ty, ny = p.tx; L.push([p.x + nx * p.w, p.y + ny * p.w]); R.push([p.x - nx * p.w, p.y - ny * p.w]); });
  const h = spine[spine.length - 1], t = spine[0], o = [...L];
  if (capHead) { const a = Math.atan2(h.ty, h.tx); for (let i = 1; i < 8; i++) { const q = a + Math.PI / 2 - Math.PI * i / 8; o.push([h.x + Math.cos(q) * h.w, h.y + Math.sin(q) * h.w]); } }
  o.push(...R.reverse());
  if (capTail) { const a = Math.atan2(-t.ty, -t.tx); for (let i = 1; i < 8; i++) { const q = a + Math.PI / 2 - Math.PI * i / 8; o.push([t.x + Math.cos(q) * t.w, t.y + Math.sin(q) * t.w]); } }
  return poly(o, true);
}
