// Toro shared: the robot bull rig, head-on. Motion follows the notes: k.t is seconds since the last note-on,
// k.gate is true while it is held, k.n counts notes (the buck alternates sides).
const TKL = [
  ['decor', 'none', '#3a3846', 1.5], ['decorS', 'none', '#625d77', 1.5], ['shadow', '#16141d', 'none', 0], ['base', '#24222e', '#b9b9c9', 1.5],
  ['cyl', '#24222e', '#b9b9c9', 1.5], ['rod', '#00ed95', 'none', 0], ['body', '#1f1d28', '#b9b9c9', 1.5], ['bodyLn', 'none', '#625d77', 1.5],
  ['grip', 'none', '#b9b9c9', 2], ['rider', 'none', '#b9b9c9', 1.5], ['riderD', 'none', '#625d77', 1], ['rLasso', 'none', '#ff423f', 1.5], ['rEye', 'none', '#ffffff', 1.5], ['rHat', 'none', '#698eff', 1.5], ['rHatD', 'none', '#4560cc', 1], ['horn', '#698eff', 'none', 0], ['hornSeg', 'none', '#4560cc', 1.2], ['hornTip', '#c4d2ff', 'none', 0],
  ['ear', '#2b2836', '#b9b9c9', 1.5], ['riderL', 'none', '#b9b9c9', 1.5], ['riderLD', 'none', '#625d77', 1], ['rBoot', 'none', '#00ed95', 1.5], ['rBootD', 'none', '#00ed95', 1], ['head', '#2b2836', '#b9b9c9', 2], ['facet', 'none', '#625d77', 1.5],
  ['sock', '#000000', 'none', 0], ['eye', '#ffffff', 'none', 0], ['glow', 'none', '#ffffff', 1.5], ['muzzle', '#1b1922', '#b9b9c9', 1.5],
  ['vent', '#000000', 'none', 0], ['steam', 'none', '#b9b9c9', 1.5], ['ring', 'none', '#ff423f', 2], ['ringFx', 'none', '#ff423f', 1.5],
  ['spark', 'none', '#00ed95', 1.5], ['lane', 'none', '#00ed95', 1.5], ['box', 'none', '#625d77', 1.5]];
const HK = 1.17;
function noteEnv(k) { return Math.exp(-k.t * 5); }        // the kick of a note: 1 at note-on, settling over ~0.5 s
function buck(k) {                                        // FOLD sets how hard each note makes the rig jump and roll
  const e = noteEnv(k), dir = k.n % 2 ? 1 : -1;
  return { lift: 2 + k.fold * 5 + k.fold * 7 * e, a: dir * k.fold * 0.07 * e * Math.cos(k.t * 9) };
}
function hornSpine(side, k, rise) {
  const sp = k.sprd, curl = k.page2 ? k.shape : 0.3;
  const P0 = [18, -18], P1 = [32 + sp * 10, -21], P2 = [42 + sp * 20, -21 - (4 + sp * 3) * rise], P3 = [44 + sp * 25 - curl * 12, -18 - (16 + sp * 4) * rise + curl * 8];
  const f = p => [p[0] * side, p[1]];
  return track([f(P0), ...bez(f(P0), f(P1), f(P2), f(P3), 24)]);
}
function drawHorn(g, side, k, T, rise) {
  const tr = hornSpine(side, k, rise), sp = [], N = 28;
  for (let j = 0; j <= N; j++) { const p = at(tr, tr.len * j / N), u = j / N; p.w = 5.4 - 3.4 * Math.pow(u, 1.3); p.u = u; sp.push(p); }
  put(g, 'horn', xf(tube(sp, true, false), T));
  put(g, 'hornTip', xf(tube(sp.filter(p => p.u > 0.84), true, false), T));
  [9, 15, 21].forEach(j => { const p = sp[j], nx = -p.ty, ny = p.tx; put(g, 'hornSeg', xf(line(p.x + nx * p.w * 0.9, p.y + ny * p.w * 0.9, p.x - nx * p.w * 0.9, p.y - ny * p.w * 0.9), T)); });
}
// o: {cx, top, a, S, rise, body, ram:{base}} ; rig units: origin on the ram top, head centre 30 above it
function drawBull(g, k, o) {
  const S = o.S, a = o.a || 0, T = { x: o.cx, y: o.top, a, s: S }, hy = -30, h = (x, y) => [x * HK, y * HK + hy];
  const HP = pts => poly(pts.map(p => h(p[0], p[1])), true), rise = o.rise === undefined ? 1 : o.rise, e = noteEnv(k);
  if (o.ram) {
    const b = o.ram.base, w = 8 * S, rx = o.ramX === undefined ? o.cx : o.ramX, rw = 3.5 * S, by = b - 13 * S;
    put(g, 'shadow', ellipse(o.cx + a * 50 * S, b + 5 * S, (40 - (b - o.top) * 0.4) * S, 5 * S));
    put(g, 'base', ellipse(rx, b, 26 * S, 4.5 * S));
    put(g, 'cyl', rr(rx - w, b - 15 * S, 2 * w, 15 * S, 2));
    put(g, 'rod', poly([[rx - rw, by], [o.cx - rw, o.top], [o.cx + rw, o.top], [rx + rw, by]], true));   // the rod leans as the rig sways
  }
  if (o.body !== false) {
    put(g, 'body', xf([['M', -36, -40], ['C', -26, -50, 26, -50, 36, -40], ['C', 47, -28, 47, -8, 34, -2], ['L', -34, -2], ['C', -47, -8, -47, -28, -36, -40], ['Z']], T));
    [-1, 1].forEach(sd => put(g, 'bodyLn', xf([['M', 30 * sd, -44], ['C', 40 * sd, -36, 42 * sd, -18, 34 * sd, -4]], T)));
    put(g, 'grip', xf([['M', -6, -52], ['C', -6, -64, 6, -64, 6, -52]], T));
  }
  if (o.rider) drawRider(g, k, T);
  const HT = { x: o.cx - hy * S * Math.sin(a), y: o.top + hy * S * Math.cos(a), a, s: S * HK };
  drawHorn(g, -1, k, HT, rise); drawHorn(g, 1, k, HT, rise);
  [-1, 1].forEach(sd => put(g, 'ear', xf(HP([[25 * sd, -12], [41 * sd, -11], [44 * sd, -6], [40 * sd, -2], [25 * sd, -2]]), T)));
  put(g, 'head', xf(HP([[-20, -22], [20, -22], [27, -10], [24, 4], [17, 16], [14, 28], [8, 32], [-8, 32], [-14, 28], [-17, 16], [-24, 4], [-27, -10]]), T));
  put(g, 'facet', xf(poly([h(-27, -10), h(-10, -7)]), T)); put(g, 'facet', xf(poly([h(27, -10), h(10, -7)]), T));
  const eh = 1.4 + k.cut * 3.4;                           // CUTOFF opens the eye slits; a note flashes a glow round them
  [-1, 1].forEach(sd => {
    put(g, 'sock', xf(HP([[21 * sd, -9], [8 * sd, -5.2], [8 * sd, 1.5], [20 * sd, -1.8]]), T));
    put(g, 'eye', xf(HP([[19.5 * sd, -7.8], [9.3 * sd, -4.6], [9.3 * sd, -4.6 + eh], [18.8 * sd, -7.8 + eh * 0.9]]), T));
    if (e * k.cut > 0.25) { const q = 1.8; put(g, 'glow', xf(HP([[(21 + q) * sd, -9 - q * 0.7], [(8 - q * 0.4) * sd, -5.2 - q], [(8 - q * 0.4) * sd, 1.5 + q * 0.8], [(20 + q) * sd, -1.8 + q * 0.6]]), T)); }
  });
  put(g, 'muzzle', xf(poly([h(-12, 13), h(12, 13), h(14, 24), h(9, 31), h(-9, 31), h(-14, 24)], true), T));
  [-1, 1].forEach(sd => put(g, 'vent', xf(HP([[3.5 * sd, 18], [9 * sd, 18], [8 * sd, 23], [4 * sd, 23]]), T)));
  if (k.page2 && k.t < 0.6) {                             // DRIVE: each note blasts steam from the vents
    const puffs = 1 + Math.round(k.drive * 3), t = k.t / 0.6;
    [-1, 1].forEach(sd => { for (let i = 0; i < puffs; i++) {
      const tt = Math.min(1, t + i * 0.18), x = 10 + tt * (14 + k.drive * 10), y = 22 + tt * 6, r = 1.6 + tt * 3;
      put(g, 'steam', xf([['M', ...h((x - r) * sd, y)], ['C', ...h((x - r) * sd, y - r * 1.3), ...h((x + r) * sd, y - r * 1.3), ...h((x + r) * sd, y)]], T));
    } });
  }
  // RES: each note strikes the nose ring; resonance sets how long it keeps swinging
  const tau = 0.12 + k.res * 1.4, swing = 0.55 * Math.exp(-k.t / tau) * Math.sin(k.t * 14), R = (4 + k.res * 2.5) * HK;
  const pv = h(0, 30), rc = [pv[0] + Math.sin(swing) * R, pv[1] + Math.cos(swing) * R];
  put(g, 'ring', xf(ellipse(rc[0], rc[1], R, R * 0.92), T));
  if (Math.abs(swing) > 0.12 && k.res > 0.4) [-1, 1].forEach(sd => put(g, 'ringFx', xf(poly(arcPts(rc[0], rc[1], R + 3, R + 3, (sd > 0 ? 0 : Math.PI) - 0.35, (sd > 0 ? 0 : Math.PI) + 0.35, 4)), T)));
  return { T, h };
}
function sparks(g, k, x0, y0, w, h) {                     // NOISE: sparks while a note is held
  if (!k.page2 || !k.gate) return;
  const r = rng(5 + k.n * 7 + Math.floor(k.t * 12));
  for (let i = 0; i < Math.round(k.noise * 10); i++) { const x = x0 + r() * w, y = y0 + r() * h; put(g, 'spark', line(x - 1.6, y, x + 1.6, y)); put(g, 'spark', line(x, y - 1.6, x, y + 1.6)); }
}
const TSC = {
  bench: { layout: 'AL', draw(g, k) {                     // 1. full rig on its ram; readout panel on the left
    const b = buck(k), base = 129, cx = 224;
    put(g, 'decor', ellipse(cx, base + 5, 82, 7));
    drawBull(g, k, { cx, top: base - 16 - b.lift, a: b.a, S: 0.98, ram: { base } }); sparks(g, k, cx - 40, base - 14, 80, 16);
  } },
  faceplate: { layout: 'B', draw(g, k) {                  // 2. a close-up of the faceplate, horns swept wide; footer lanes
    const e = noteEnv(k), dir = k.n % 2 ? 1 : -1;
    drawBull(g, k, { cx: 160, top: 61 + k.fold * 3 * e, a: dir * k.fold * 0.05 * e * Math.cos(k.t * 9), S: 0.9, rise: 0.1, body: false });
    sparks(g, k, 110, 64, 100, 16);
  } },
  arena: { layout: 'C', draw(g, k) {                      // 3. the bull in a ring of rails; readouts split left and right
    const b = buck(k), base = 128, cx = 160;
    put(g, 'decorS', ellipse(cx, base + 6, 70, 8)); put(g, 'decor', line(92, 96, 92, 134)); put(g, 'decor', line(228, 96, 228, 134));
    put(g, 'decor', line(92, 104, 102, 104)); put(g, 'decor', line(218, 104, 228, 104));
    drawBull(g, k, { cx, top: base - 15 - b.lift, a: b.a, S: 0.82, ram: { base } }); sparks(g, k, cx - 36, base - 12, 72, 14);
  } },
  operator: { layout: 'D2', draw(g, k) {                  // 4. the operator's box holds the readouts; its cable runs to the ram
    const b = buck(k), base = 129, cx = 98;
    put(g, 'decor', ellipse(cx, base + 5, 80, 7));
    put(g, 'lane', [['M', cx + 24, base + 2], ['C', 150, base + 6, 170, 118, 191, 116]]);   // FOLD's hydraulic line pulses with each note
    if (k.t < 0.4) { const u = k.t / 0.4, p = bez([cx + 24, base + 2], [150, base + 6], [170, 118], [191, 116], 20)[Math.min(17, Math.floor((1 - u) * 18))]; put(g, 'rod', circle(p[0], p[1], 2.2)); }
    drawBull(g, k, { cx, top: base - 16 - b.lift, a: b.a, S: 0.94, ram: { base } }); sparks(g, k, cx - 40, base - 14, 80, 16);
  } },
  rodeo: { layout: 'E', draw(g, k) {                      // 5. a wide rodeo floor under a scoreboard header
    const b = buck(k), base = 131, cx = 160;
    put(g, 'decor', ellipse(cx, base + 4, 140, 6));
    [30, 70, 250, 290].forEach(x => put(g, 'decor', line(x, 100, x, 136))); put(g, 'decor', line(22, 108, 78, 108)); put(g, 'decor', line(242, 108, 298, 108));
    drawBull(g, k, { cx, top: base - 11 - b.lift * 0.7, a: b.a, S: 0.62, ram: { base } }); sparks(g, k, cx - 30, base - 10, 60, 12);
  } }
};
function toroScene(name, k) { const g = grp(TKL); TSC[name].draw(g, k); return out(g, TKL); }
// ---- refined round: five takes on the operator box ----
function hose(g, k, pts, rev) {                           // a hydraulic line; each note sends a pulse along it (FOLD owns it)
  put(g, 'lane', [['M', ...pts[0]], ['C', ...pts[1], ...pts[2], ...pts[3]]]);
  if (k.t < 0.4) { const u = k.t / 0.4, b = bez(pts[0], pts[1], pts[2], pts[3], 20), p = b[Math.max(0, Math.min(18, Math.floor((rev ? u : 1 - u) * 19)))]; put(g, 'rod', circle(p[0], p[1], 2 + k.fold)); }
}
Object.assign(TSC, {
  floor: { layout: 'D2', draw(g, k) {                     // O1: the hose runs along the floor from the box
    const b = buck(k), base = 129, cx = 96;
    put(g, 'decor', ellipse(cx, base + 5, 82, 7)); hose(g, k, [[cx + 20, base + 4], [140, base + 10], [178, base + 6], [190, 132]]);
    put(g, 'decorS', line(190, 132, 194, 132));
    drawBull(g, k, { cx, top: base - 16 - b.lift, a: b.a, S: 0.94, ram: { base } }); sparks(g, k, cx - 40, base - 14, 80, 16);
  } },
  left: { layout: 'D2L', draw(g, k) {                     // O2: box on the left, bull on the right
    const b = buck(k), base = 129, cx = 222;
    put(g, 'decor', ellipse(cx, base + 5, 82, 7)); hose(g, k, [[132, 116], [150, 118], [168, base + 8], [cx - 20, base + 4]], true);
    drawBull(g, k, { cx, top: base - 16 - b.lift, a: b.a, S: 0.94, ram: { base } }); sparks(g, k, cx - 40, base - 14, 80, 16);
  } },
  strip: { layout: 'DF', draw(g, k) {                     // O3: a console strip under the bull's faceplate
    const e = noteEnv(k), dir = k.n % 2 ? 1 : -1;
    put(g, 'decor', line(16, 78, 304, 78));
    drawBull(g, k, { cx: 160, top: 58 + k.fold * 2 * e, a: dir * k.fold * 0.05 * e * Math.cos(k.t * 9), S: 0.84, rise: 0.1, body: false });
    sparks(g, k, 110, 60, 100, 14);
  } },
  pendant: { layout: 'D', draw(g, k) {                    // O4: a hanging pendant control, its hose running to the ram
    const b = buck(k), base = 129, cx = 92;
    put(g, 'decor', ellipse(cx, base + 5, 80, 7)); hose(g, k, [[172, 120], [166, 128], [150, base + 8], [cx + 20, base + 4]], true);
    drawBull(g, k, { cx, top: base - 16 - b.lift, a: b.a, S: 0.92, ram: { base } }); sparks(g, k, cx - 40, base - 14, 80, 16);
  } },
  twin: { layout: 'DC', draw(g, k) {                      // O5: two boxes, one per encoder pair, hoses from both sides
    const b = buck(k), base = 128, cx = 160;
    put(g, 'decor', ellipse(cx, base + 6, 62, 7));
    hose(g, k, [[96, 112], [104, 120], [120, base + 6], [cx - 14, base + 3]], true); hose(g, k, [[224, 112], [216, 120], [200, base + 6], [cx + 14, base + 3]], true);
    drawBull(g, k, { cx, top: base - 15 - b.lift, a: b.a, S: 0.74, ram: { base } }); sparks(g, k, cx - 34, base - 12, 68, 14);
  } }
});

// ---- the cowboy: line art only (no fills), sitting behind the head. m is how hard the ride is right now:
// FOLD sets how far each note throws the bull, so the rider's legs flail, his torso whips and his face goes from a
// smile to an O with wide eyes as m rises.
function limb(pts, w0, w1) {                              // an outlined limb through 2-3 joints, tapering w0 -> w1
  const sp = [], N = 10 * (pts.length - 1);
  for (let s = 0; s < pts.length - 1; s++) for (let j = 0; j <= 10; j++) {
    if (s > 0 && j === 0) continue;
    const a = pts[s], b = pts[s + 1], t = j / 10, x = a[0] + (b[0] - a[0]) * t, y = a[1] + (b[1] - a[1]) * t, L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1;
    sp.push({ x, y, tx: (b[0] - a[0]) / L, ty: (b[1] - a[1]) / L, w: w0 + (w1 - w0) * (sp.length / N) });
  }
  return tube(sp, true, true);
}
function drawRider(g, k, T) {
  const e = noteEnv(k), m = Math.min(1, k.fold * (0.2 + 0.9 * e) * 1.25), t = k.t + k.n * 0.5, dir = k.n % 2 ? 1 : -1;
  const wild = k.fold * k.fold;                                             // the top of the FOLD range gets much rougher
  const rt = -(T.a || 0) * (1.8 + 2 * wild) + dir * m * (0.12 + 0.4 * wild) * Math.sin(k.t * 11);   // the torso whips against the buck
  const up = Math.abs(Math.sin(k.t * 9)) * m * (3 + 4 * wild);              // bounced out of the saddle
  const slide = dir * m * wild * 7 * Math.sin(k.t * 9 + 0.6);               // and thrown side to side
  const RS = 1.55, HS = 1.7, BS = 1.75;                   // rider scale; the hat and boots are comically oversized on top of that
  const W = (shape, tilt) => xf(xf(shape, { a: tilt === false ? 0 : rt, px: 0, py: -52, s: RS, x: slide * (tilt === false ? 0.4 : 1), y: -up * (tilt === false ? 0.3 : 1) }), T);
  const big = (shape, cx0, cy0, f) => xf(shape, { px: cx0, py: cy0, s: f });
  // legs: from the hips out over the bull's shoulders; knees and boots swing on each throw
  [-1, 1].forEach(sd => {
    const ph = t * 13 + (sd > 0 ? 0 : 1.7), kick = m * (4 + 6 * e + 9 * wild * e);
    const hip = [7 * sd, -55], knee = [(24 + Math.sin(ph) * kick * 0.4) * sd, -50 - Math.cos(ph) * kick * 0.35 - m * 3];
    const ank = [(33 + Math.sin(ph + 0.8) * kick * 0.75) * sd, -38 - Math.cos(ph + 0.8) * kick * 0.7 - m * 4];
    put(g, 'riderL', W(limb([hip, knee, ank], 2.6, 2), false));
    const bx = ank[0], by = ank[1], o = sd;                                   // boot: shaft, toe out, heel, spur
    put(g, 'rBoot', W(big([['M', bx - 2.4 * o, by - 1], ['L', bx - 2.6 * o, by + 6], ['C', bx - 2.6 * o, by + 8, bx + 1 * o, by + 8.5, bx + 6.5 * o, by + 8], ['C', bx + 7.5 * o, by + 7.8, bx + 7.5 * o, by + 6, bx + 5.5 * o, by + 5.6], ['L', bx + 2.4 * o, by + 4.5], ['L', bx + 2.4 * o, by - 1], ['Z']], bx, by, BS), false));
    put(g, 'rBootD', W(big(line(bx - 2.6 * o, by + 2.4, bx + 2.4 * o, by + 2.4), bx, by, BS), false));
    const hx = bx - 2.6 * o, hy = by + 6.5, sx = hx - 4 * o, spin = t * 20;  // spur: a strap, a shank and a spinning rowel
    put(g, 'rBootD', W(big(line(hx, hy, sx + 1.6 * o, hy), bx, by, BS), false));
    put(g, 'rBoot', W(big(circle(sx, hy, 1.7), bx, by, BS), false));
    for (let q = 0; q < 4; q++) { const a = spin + q * Math.PI / 4; put(g, 'rBootD', W(big(line(sx + Math.cos(a) * 2.8, hy + Math.sin(a) * 2.8, sx - Math.cos(a) * 2.8, hy - Math.sin(a) * 2.8), bx, by, BS), false)); }
  });
  // torso, belt and bandana
  put(g, 'rider', W([['M', -6, -54], ['L', -9.5, -73], ['C', -6, -75, 6, -75, 9.5, -73], ['L', 6, -54], ['Z']]));
  put(g, 'riderD', W(line(-6.3, -57.5, 6.3, -57.5))); put(g, 'riderD', W(rr(-2.2, -59.2, 4.4, 3.4, 0.8)));
  put(g, 'rider', W(poly([[-5, -74], [5, -74], [0, -68.5]], true))); put(g, 'riderD', W(line(0, -68, 0, -60)));   // bandana
  // arms: the left grips the handle, the right waves free and swings harder with the ride
  put(g, 'rider', W(limb([[-8.6, -71], [-14, -63], [-5, -61]], 1.9, 1.6))); put(g, 'rider', W(circle(-4.2, -61, 1.7)));
  // RES raises the free arm: at zero it rests on his thigh; turned up he swings a lasso that grows with it
  const rr0 = k.res, wave = Math.sin(t * (10 + 6 * wild)) * (0.1 + 0.25 * rr0 + m * (0.25 + 0.3 * wild) * rr0);
  const lerp = (p, q) => [p[0] + (q[0] - p[0]) * rr0, p[1] + (q[1] - p[1]) * rr0];
  const el = lerp([12.5, -64], [18, -80]), hand0 = lerp([11, -57], [25, -88]);
  const hand = [hand0[0] + Math.sin(wave) * 4 * rr0, hand0[1] - Math.cos(wave) * 1.5 * rr0 + (1 - rr0) * Math.sin(wave) * 1.2];
  put(g, 'rider', W(limb([[8.6, -71], el, hand], 1.9, 1.6))); put(g, 'rider', W(circle(hand[0], hand[1], 1.8)));
  if (rr0 > 0.03) {                                       // each note flings the loop wide; resonance sets how long it stays open
    const tau = 0.12 + rr0 * 1.4, live = Math.exp(-k.t / tau), spin = k.t * 14 + k.n;
    const R = (3 + rr0 * 29) * (0.6 + 0.4 * live), cxl = hand[0] + R * 0.6, cyl = hand[1] - 2 - R * 0.12;
    const p = [cxl + Math.cos(spin) * R, cyl + Math.sin(spin) * R * 0.42];
    let loop = W(ellipse(cxl, cyl, R, R * 0.42)), knot = W(circle(p[0], p[1], 0.9));
    const ys = loop.flatMap(c => c.slice(1).filter((v, i) => i % 2)), dy = Math.max(0, 9.5 - Math.min(...ys));   // a thrown loop dips rather than leave the screen
    if (dy) { loop = xf(loop, { y: dy }); knot = xf(knot, { y: dy }); }
    const hs = W([['M', hand[0], hand[1]]])[0], kp = knot[0];
    put(g, 'rLasso', loop); put(g, 'rLasso', knot);
    put(g, 'rLasso', [['M', hs[1], hs[2]], ['Q', (hs[1] + kp[1]) / 2 + 2, Math.max(hs[2], kp[2]) + 2, kp[1] - 0.9, kp[2]]]);
  }
  // head and face: a smile while it's calm, an O with wide eyes as the bull throws him
  put(g, 'rider', W(circle(0, -81.5, 6.6)));
  const er = 0.5 + k.cut * 0.45 + m * 0.8;          // CUT opens his eyes as it opens the bull's; the ride widens them
  [-1, 1].forEach(sd => { put(g, 'rEye', W(circle(2.5 * sd, -82.6, er))); put(g, 'riderD', W(line(1.2 * sd, -85 - m * 1.3, 3.8 * sd, -84.8 - m * 1.6 + 0.6))); });
  if (m < 0.3) put(g, 'rider', W([['M', -2.6, -78.8], ['Q', 0, -76.6, 2.6, -78.8]]));
  else put(g, 'rider', W(ellipse(0, -78, 0.7 + m * 0.7, 0.9 + m * 1.1)));
  // the big hat: a wide brim and a dented crown, lifting off his head on the hardest throws
  const hl = m * e * (2 + 1.5 * wild) + 0.6;
  const bw = 0.8 + k.sprd * 0.45, bc = (k.page2 ? k.shape : 0.3) * 3;    // SPREAD widens the brim like the horns; SHAPE curls its ends
  put(g, 'rHat', W(big([['M', -16 * bw, -86.5 - hl - bc], ['C', -12 * bw, -83.8 - hl, 12 * bw, -83.8 - hl, 16 * bw, -86.5 - hl - bc], ['C', 12 * bw, -86 - hl, -12 * bw, -86 - hl, -16 * bw, -86.5 - hl - bc], ['Z']], 0, -87, HS)));
  put(g, 'rHat', W(big([['M', -7, -85.6 - hl], ['C', -8, -92 - hl, -6.5, -96.5 - hl, -3, -96 - hl], ['C', -1, -94.6 - hl, 1, -94.6 - hl, 3, -96 - hl], ['C', 6.5, -96.5 - hl, 8, -92 - hl, 7, -85.6 - hl]], 0, -87, HS)));
  put(g, 'rHatD', W(big(line(-7.3, -88 - hl, 7.3, -88 - hl), 0, -87, HS)));
}
TSC.rider = { layout: 'TK', draw(g, k) {                   // the final: bull with a rider; the hose runs to the FOLD readout
  const b = buck(k), base = 132, cx0 = 150; b.lift *= 0.4;
  const on = k.t < 1 ? 1 : 0, dir = k.n % 2 ? 1 : -1, e2 = Math.exp(-k.t * 3);          // FOLD: the bull rocks hard, and keeps rocking while notes play
  b.a = on * dir * (0.08 * k.fold + 0.2 * k.fold * k.fold) * (0.35 + 0.65 * e2) * Math.cos(k.t * 9);
  const cx = cx0 + b.a * 22;
  put(g, 'decor', ellipse(cx0, base + 4, 78, 5.5)); hose(g, k, [[cx0 + 20, base + 4], [200, base + 10], [236, base + 6], [256, 131]]);
  put(g, 'decorS', line(256, 131, 261, 131));
  const o = { cx, top: base - 16 - b.lift, a: b.a, S: 0.7, ram: { base }, ramX: cx0, rider: true };
  drawBull(g, k, o); sparks(g, k, cx - 40, base - 14, 80, 16);
} };
