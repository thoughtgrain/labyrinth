const COLS = ['#698eff', '#00ed95', '#ffffff', '#ff423f'];
const PAGE = ['CORR', 'LEN', 'DENS', 'RNG'], SHIFT = ['RATE', 'GATE', 'SCL', 'HOLD'];
const DESC = {
  CORR: 'Chance the playhead rewrites the step it lands on. A rewritten step is marked with a diamond for one pass.',
  LEN: 'Loop length, 2 to 16 steps. A gate (a bar with end posts) blocks the corridor; the thread rewinds there.',
  DENS: 'How many steps play. Each round bead is a note; solid beads are accents.',
  RNG: 'Pitch spread. A wedge points from the thread to each bead: the longer the wedge, the further from the root.',
  RATE: 'Step rate. Sets how fast the thread runs from bead to bead.',
  GATE: 'Note length. A ring circles the playing bead for this share of the step.',
  SCL: 'The scale pitches snap to, shown in the readout.',
  HOLD: 'Latch. On: a square-headed pin goes through the ball and the loop keeps playing after the key is released.' };
const RATES = ['1/4', '1/8', '8T', '1/16', '16T', '1/32'], RATE_MS = [520, 300, 220, 180, 140, 110];
const SCALES = ['KEY', 'MAJ', 'MIN', 'DOR', 'PHR', 'MIX', 'PN+', 'PN-', 'BLU', 'CHR'];
class Component extends DCLogic {
  constructor(props) { super(props); this.st = meikyuInit(7); this.state = { p: [30, 71, 60, 60], s: [60, 45, 22, 0], shift: false, key: true, frame: 0 }; }
  componentDidMount() {
    this.alive = true;
    const step = () => {
      if (!this.alive) return;
      const k = this.knobs();
      if (k.running) meikyuStep(this.st, k); else { this.st.step = meikyuLen(k) - 1; this.st.age = 99; }   // stopped: the next press starts at step 1
      this.stepT = setTimeout(step, RATE_MS[this.rate()]);
    };
    const anim = () => { if (!this.alive) return; meikyuTick(this.st); this.setState({ frame: this.state.frame + 1 }); this.animT = setTimeout(anim, 60); };
    this.stepT = setTimeout(step, 400); this.animT = setTimeout(anim, 60);
  }
  componentWillUnmount() { this.alive = false; clearTimeout(this.stepT); clearTimeout(this.animT); }
  rate() { return Math.round(this.state.s[0] / 100 * 5); }
  gate() { return 10 + Math.round(this.state.s[1] / 100 * 18) * 5; }
  knobs() {
    const p = this.state.p, hold = this.state.s[3] >= 50, sf = RATE_MS[this.rate()] / 60;
    return { corr: p[0] / 100, len: p[1] / 100, dens: p[2] / 100, rng: p[3] / 100, hold, running: this.state.key || hold, stepFrames: sf, gateFrames: sf * this.gate() / 100 };
  }
  renderVals() {
    const p = this.state.p, s = this.state.s, sh = this.state.shift, k = this.knobs();
    const main = [[String(Math.round(p[0] / 5) * 5), '%'], [String(meikyuLen(k)), ''], [String(Math.round(p[2] / 5) * 5), '%'], [String(Math.round(p[3] / 100 * 15)), '']];
    const shift = [[RATES[this.rate()], ''], [String(this.gate()), '%'], [SCALES[Math.round(s[2] / 100 * 9)], ''], [s[3] >= 50 ? 'ON' : 'OFF', '']];
    const names = sh ? SHIFT : PAGE, vals = sh ? shift : main, src = sh ? s : p, key = sh ? 's' : 'p';
    const r = { g: threadDraw(k, this.st) };
    Object.assign(r.g, iconPaths('MK', names));
    names.forEach((nm, i) => { r['n' + i] = nm; r['v' + i] = vals[i][0]; r['u' + i] = vals[i][1]; });
    r.dials = names.map((nm, i) => ({ name: nm, color: COLS[i], v: src[i], show: vals[i][0] + vals[i][1],
      set: e => { const a = this.state[key].slice(); a[i] = Number(e.target.value); this.setState({ [key]: a }); } }));
    r.legend = names.map((nm, i) => ({ name: nm, color: COLS[i], desc: DESC[nm] }));
    r.toggle = () => this.setState({ shift: !sh }); r.toggleLabel = sh ? 'Release Shift' : 'Hold Shift'; r.pressed = sh ? 'true' : 'false';
    r.toggleBg = sh ? '#b9b9c9' : 'transparent'; r.toggleFg = sh ? '#0e0e10' : '#b9b9c9';
    r.toggle2 = () => this.setState({ key: !this.state.key }); r.toggle2Label = this.state.key ? 'Release the key' : 'Hold a key';
    r.pressed2 = this.state.key ? 'true' : 'false'; r.toggle2Bg = this.state.key ? '#b9b9c9' : 'transparent'; r.toggle2Fg = this.state.key ? '#0e0e10' : '#b9b9c9';
    r.state = k.running ? (this.state.key ? 'Key held: the loop plays.' : 'Key released, HOLD on: the loop keeps playing.') : 'Key released: the thread rewinds to the gate and waits.';
    return r;
  }
}
