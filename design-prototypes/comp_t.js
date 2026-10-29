const COLS = ['#698eff', '#00ed95', '#ffffff', '#ff423f'];
const PAGE1 = ['SPRD', 'FOLD', 'CUT', 'RES'], PAGE2 = ['SHAP', 'NOIS', 'DRIV', 'RES'];
const DESC = {
  SPRD: 'Detune between the two oscillators. The horns spread wider, and so does the brim of his blue hat.',
  FOLD: 'Wavefolder amount. The bull rocks harder on every note: the hose pulses, the cowboy gets thrown about, his green boots and spurs kick, and his face goes from a smile to an O.',
  CUT: "Filter cutoff. The bull's eye slits open, and so do the cowboy's eyes; a note flashes a glow round the slits when it is high.",
  RES: 'Resonance. Each note strikes the nose ring. At zero his free arm rests; turn it up and he raises an orange lasso that grows with it. More resonance, the longer the ring and the loop keep swinging.',
  SHAP: 'Oscillator shape, saw to triangle. The horn tips and the hat brim curl.',
  NOIS: 'Noise level. Sparks fly off the turntable while a note is held.',
  DRIV: 'Drive into the soft clipper. Steam blasts from the vents on each note.' };
class Component extends DCLogic {
  constructor(props) { super(props); this.state = { p: [55, 60, 60, 55], q: [40, 50, 60], page2: false, play: true, tick: 0 }; }
  componentDidMount() {
    this.alive = true;
    const run = () => { if (!this.alive) return; this.setState({ tick: this.state.tick + 1 }); this.timer = setTimeout(run, 50); };
    this.timer = setTimeout(run, 200);
  }
  componentWillUnmount() { this.alive = false; clearTimeout(this.timer); }
  renderVals() {
    const p = this.state.p, q = this.state.q, p2 = this.state.page2, tk = this.state.tick, pl = this.state.play;
    const k = { sprd: p[0] / 100, fold: p[1] / 100, cut: p[2] / 100, res: p[3] / 100, shape: q[0] / 100, noise: q[1] / 100, drive: q[2] / 100, page2: p2,
      t: pl ? (tk % 10) * 0.05 : 9, gate: pl && tk % 10 < 6, n: Math.floor(tk / 10) };   // a demo note every half second, held 0.3 s
    const names = p2 ? PAGE2 : PAGE1, src = p2 ? [q[0], q[1], q[2], p[3]] : p, show = v => String(Math.round(v * 0.99));
    const r = { g: toroScene('rider', k) };
    Object.assign(r.g, iconPaths('TK', names));
    names.forEach((nm, i) => { r['n' + i] = nm; r['v' + i] = show(src[i]); r['u' + i] = ''; });
    r.dials = names.map((nm, i) => ({ name: nm, color: COLS[i], v: src[i], show: show(src[i]),
      set: e => { const v = Number(e.target.value); if (p2 && i < 3) { const a = this.state.q.slice(); a[i] = v; this.setState({ q: a }); } else { const a = this.state.p.slice(); a[i] = v; this.setState({ p: a }); } } }));
    r.legend = names.map((nm, i) => ({ name: nm, color: COLS[i], desc: DESC[nm] }));
    r.toggle = () => this.setState({ page2: !p2 }); r.toggleLabel = p2 ? 'Back to page 1' : 'Press for page 2'; r.pressed = p2 ? 'true' : 'false';
    r.toggleBg = p2 ? '#b9b9c9' : 'transparent'; r.toggleFg = p2 ? '#0e0e10' : '#b9b9c9';
    r.toggle2 = () => this.setState({ play: !pl }); r.toggle2Label = pl ? 'Stop the demo notes' : 'Play demo notes';
    r.pressed2 = pl ? 'true' : 'false'; r.toggle2Bg = pl ? '#b9b9c9' : 'transparent'; r.toggle2Fg = pl ? '#0e0e10' : '#b9b9c9';
    r.state = pl ? 'A demo note plays every half second; all motion follows it.' : 'No notes: the bull and rider settle.';
    return r;
  }
}
