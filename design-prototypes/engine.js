// Meikyu model (the plugin's shift-register sequencer, simplified for the preview) and the shared paint layers.
// Encoder ownership on the main page: CORR blue, LEN green, DENS white (the notes), RNG orange. Shift: HOLD orange.
function meikyuInit(seed) {
  const r = rng(seed || 7), st = { val: [], wt: [], flip: 1 << 9, step: 0, bites: [], r, age: 99, rows: [], cur: Array(16).fill(0), turns: 0 };
  for (let i = 0; i < 16; i++) { st.val.push(Math.floor(r() * 16)); st.wt.push(Math.floor(r() * 100)); }
  return st;
}
function meikyuLen(k) { return 2 + Math.round(k.len * 14); }
function mPlays(st, k, i) { const on = st.wt[i] < k.dens * 100; return (st.flip >> i) & 1 ? !on : on; }
function mAccent(st, k, i) { return st.wt[i] < k.dens * 25; }
function mDeg(st, k, i) { return st.val[i] * Math.round(k.rng * 15) / 15; }   // 0..15 scale degrees
function meikyuStep(st, k) {
  const len = meikyuLen(k);
  st.step = (st.step + 1) % len; st.age = 0;
  if (st.step === 0) { st.rows.unshift(st.cur.slice(0, 16)); st.rows = st.rows.slice(0, 6); st.cur = Array(16).fill(0); st.turns++; }
  st.bites = st.bites.map(b => ({ i: b.i, age: b.age + 1 })).filter(b => b.age < len && b.i < len);
  if (st.r() < k.corr) { const i = st.step; st.val[i] = Math.floor(st.r() * 16); st.wt[i] = Math.floor(st.r() * 100); st.flip &= ~(1 << i); st.bites.push({ i, age: 0 }); }
  st.cur[st.step] = mPlays(st, k, st.step) ? (mAccent(st, k, st.step) ? 2 : 1) : 0;
}
function meikyuTick(st) { st.age++; }                    // once per animation frame (60 ms)
function bitten(st, i) { const b = st.bites.find(x => x.i === i); return b ? b.age : -1; }
function ease(st, n) { return Math.min(1, st.age / (n || 3)); }
function note(g, st, k, i, x, y, r) {                     // DENS owns the notes: white; accents filled, others outlined
  if (!mPlays(st, k, i)) return;
  put(g, mAccent(st, k, i) ? 'noteW' : 'noteO', circle(x, y, r));
}
