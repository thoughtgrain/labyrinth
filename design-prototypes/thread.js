// Meikyu final: THREAD. Ariadne's thread runs the corridors of a round labyrinth toward the bull at its centre.
// How each control shows up:
//   CORR (blue)  each time the playhead passes a step, CORR is the chance it rewrites it; a rewritten step keeps a
//                blue knot for one full pass, so more knots on screen = more corruption.
//   LEN  (green) the green bar across the corridor; the thread never passes it and rewinds to the gate at the bar.
//   DENS (white) the beads: a bead is a step that plays. Accented steps are solid, the rest outlined.
//   RNG  (orange) pitch spread: beads hang off the thread on orange stems; the stem's length and side is the pitch.
//   Shift RATE   how fast the thread moves from bead to bead.   GATE  how long the played bead stays lit.
//         SCL    the scale name in the readout.               HOLD  an orange pin through the ball keeps it playing
//                                                             after the key is released.
const ML = [
  ['dim', 'none', '#3a3846', 1.5], ['dimF', '#3a3846', 'none', 0], ['wall', 'none', '#625d77', 1.5], ['bull', 'none', '#b9b9c9', 1.5],
  ['thread', 'none', '#b9b9c9', 1.5], ['stem', '#ff423f', 'none', 0], ['lenL', 'none', '#00ed95', 2],
  ['knot', 'none', '#698eff', 1.5], ['knotNew', 'none', '#698eff', 2], ['noteO', 'none', '#ffffff', 1.5], ['noteW', '#ffffff', 'none', 0],
  ['lit', 'none', '#00ed95', 1.5], ['play', 'none', '#b9b9c9', 1.5], ['ball', 'none', '#b9b9c9', 1.5], ['hold', 'none', '#ff423f', 2]];
const TH = { cx: 160, cy: 74, R: [55, 43, 31, 19], W: [61, 49, 37, 25, 13], SP: Math.PI / 2 * 0.86 };
TH.start = [Math.PI / 2]; for (let r = 1; r < 5; r++) TH.start.push(TH.start[r - 1] + (r % 2 ? 1 : -1) * 4 * TH.SP);
const thDir = r => (r % 2 ? -1 : 1);
const thAng = u => { const r = Math.min(3, Math.floor(u / 4)); return TH.start[r] + thDir(r) * (u - r * 4) * TH.SP; };   // u: position along the path in steps
const thPt = (u, off) => { const r = Math.min(3, Math.floor(u / 4)), a = thAng(u), rad = TH.R[r] + (off || 0); return [TH.cx + Math.cos(a) * rad, TH.cy + Math.sin(a) * rad]; };
function threadPath(u) {                                  // points from the gate along the corridors to path position u
  const { cx, cy, R } = TH, pts = [[cx, 138], [cx, cy + R[0]]];
  for (let r = 0; r < 4; r++) {
    const span = Math.min(4, u - r * 4); if (span <= 0) break;
    const n = Math.max(2, Math.round(span * 6));
    for (let j = 0; j <= n; j++) pts.push(thPt(r * 4 + span * j / n * (span >= 4 && j === n ? 0.9999 : 1)));
    if (span >= 4 && r < 3) pts.push(thPt((r + 1) * 4));
  }
  return pts;
}
function threadDraw(k, st) {
  const g = grp(ML), { cx, cy, R, W, SP } = TH, len = meikyuLen(k);
  W.forEach((r, w) => { const gap = TH.start[w], ga = 7 / r; put(g, 'wall', poly(arcPts(cx, cy, r, r, gap + ga, gap + Math.PI * 2 - ga, 56))); });
  put(g, 'bull', [['M', cx - 5, cy + 2], ['C', cx - 8, cy - 3, cx - 6, cy - 6, cx - 2.5, cy - 4.5], ['M', cx + 5, cy + 2], ['C', cx + 8, cy - 3, cx + 6, cy - 6, cx + 2.5, cy - 4.5]]);
  put(g, 'bull', circle(cx, cy + 1, 2.6));
  // the thread: eases to the playhead at RATE; at the LEN bar it rewinds to the gate
  let u = 0;
  if (k.running) {
    const f = Math.min(1, st.age / Math.max(1, k.stepFrames * 0.7));
    u = st.step === 0 ? (len - 0.5) * (1 - f) + 0.5 * f : st.step - 0.5 + f;
  }
  const pts = threadPath(u); put(g, 'thread', poly(pts));
  for (let i = 0; i < 16; i++) {
    const c = thPt(i + 0.5);
    if (i >= len) { put(g, 'dimF', circle(c[0], c[1], 1.4)); continue; }
    const off = (mDeg(st, k, i) / 15 - 0.5) * 11.5, b = thPt(i + 0.5, off);
    if (mPlays(st, k, i)) {
      if (Math.abs(off) > 1.2) {                          // RNG: an orange wedge points from the thread to the bead
        const L = Math.hypot(b[0] - c[0], b[1] - c[1]), ux = (b[0] - c[0]) / L, uy = (b[1] - c[1]) / L, tip = [b[0] - ux * 2.4, b[1] - uy * 2.4];
        put(g, "stem", poly([[c[0] - uy * 2.4, c[1] + ux * 2.4], tip, [c[0] + uy * 2.4, c[1] - ux * 2.4]], true));
      }
      put(g, mAccent(st, k, i) ? 'noteW' : 'noteO', circle(b[0], b[1], 2.1));
      if (k.running && i === st.step && st.age < k.gateFrames) put(g, 'lit', circle(b[0], b[1], 4.5 + st.age * 0.4));   // GATE (green encoder on Shift): a ring while the note sounds
    }
    const bt = bitten(st, i);
    if (bt >= 0) { const d = bt < 2 ? 5.5 : 4.2; put(g, bt < 2 ? 'knotNew' : 'knot', poly([[c[0], c[1] - d], [c[0] + d, c[1]], [c[0], c[1] + d], [c[0] - d, c[1]]], true)); }   // CORR: a diamond on the rewritten step
  }
  { const r = (len - 1) >> 2, a = thAng(len - 1 + 0.5) + thDir(r) * SP * 0.5;   // LEN bar
    const p0 = [cx + Math.cos(a) * (W[r + 1] + 2), cy + Math.sin(a) * (W[r + 1] + 2)], p1 = [cx + Math.cos(a) * (W[r] - 2), cy + Math.sin(a) * (W[r] - 2)], tx = -Math.sin(a) * 2.6, ty = Math.cos(a) * 2.6;
    put(g, 'lenL', line(p0[0], p0[1], p1[0], p1[1]));                                  // LEN: a gate, a bar with end posts
    [p0, p1].forEach(q => put(g, 'lenL', line(q[0] - tx, q[1] - ty, q[0] + tx, q[1] + ty))); }
  if (k.running) { const t = pts[pts.length - 1]; put(g, 'play', circle(t[0], t[1], 4)); }
  const ballR = 5.5 - u * 0.17;                            // the ball of yarn shrinks as the thread runs out
  put(g, 'ball', circle(cx, 138 - ballR, ballR)); put(g, 'ball', line(cx - ballR * 0.6, 138 - ballR * 1.5, cx + ballR * 0.6, 138 - ballR * 0.5));
  if (k.hold) { put(g, 'hold', line(cx + 9, 140, cx - 1, 130)); put(g, 'hold', rr(cx - 4.5, 125.5, 4.5, 4.5, 0.6)); }   // HOLD: a square-headed pin through the ball
  return out(g, ML);
}
