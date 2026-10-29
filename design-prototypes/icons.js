// Readout icons: each is a miniature of the shape that setting draws on screen, so a value is tied to its
// visual by shape as well as colour. Drawn in a 10 x 10 box whose bottom sits 1.5 above the name's baseline.
const ICON_LAYERS = [0, 1, 2, 3].flatMap(i => [['icon' + i, 'none', ['#698eff', '#00ed95', '#ffffff', '#ff423f'][i], 1.5], ['icon' + i + 'f', ['#698eff', '#00ed95', '#ffffff', '#ff423f'][i], 'none', 0]]);
const ICONS = {
  CORR: [['s', poly([[5, 0.8], [9.2, 5], [5, 9.2], [0.8, 5]], true)]],                                   // the rewrite diamond
  LEN: [['s', line(1, 5, 9, 5)], ['s', line(1, 1.5, 1, 8.5)], ['s', line(9, 1.5, 9, 8.5)]],            // the gate
  DENS: [['f', circle(2.8, 5, 2.3)], ['s', circle(7.4, 5, 2)]],                                           // a solid and an outlined bead
  RNG: [['f', poly([[1, 1.2], [9.4, 5], [1, 8.8]], true)]],                                               // the pitch wedge
  RATE: [['s', line(0.8, 5, 8.6, 5)], ['s', poly([[5.8, 2.2], [9, 5], [5.8, 7.8]])]],                    // the thread running on
  GATE: [['s', circle(5, 5, 4)], ['f', circle(5, 5, 1.5)]],                                               // the ring round a bead
  SCL: [['s', poly([[0.8, 9], [3.6, 9], [3.6, 5.5], [6.4, 5.5], [6.4, 2], [9.2, 2]])]],                  // scale steps
  HOLD: [['s', rr(0.8, 0.8, 4.4, 4.4, 0.6)], ['s', line(3, 5.2, 8.8, 9.2)]],                             // the square-headed pin
  SPRD: [['s', circle(5, 7, 2.2)], ['s', [['M', 3, 6.2], ['C', 1, 6.2, 0.6, 4, 0.8, 1]]], ['s', [['M', 7, 6.2], ['C', 9, 6.2, 9.4, 4, 9.2, 1]]]],   // a head with spreading horns
  FOLD: [['s', [['M', 0.8, 8.6], ['C', 3.4, 8.6, 4.6, 3.2, 9.2, 3.2]]], ['f', circle(5.4, 5.2, 1.7)]],   // the hose and its pulse
  CUT: [['f', poly([[0.6, 3.2], [9.4, 4.6], [9, 7.6], [1, 6.2]], true)]],                                  // an eye slit
  RES: [['s', ellipse(5.8, 3.6, 3.6, 2.4)], ['s', line(1.2, 9.4, 3, 5.6)]],                               // the lasso
  SHAP: [['s', [['M', 0.8, 9], ['C', 5, 9, 9, 6.4, 8.4, 2.4], ['C', 8, 0.6, 5.2, 0.8, 5.6, 3]]]],        // a curled horn tip
  NOIS: [['s', line(5, 0.8, 5, 9.2)], ['s', line(0.8, 5, 9.2, 5)], ['s', line(2.2, 2.2, 7.8, 7.8)], ['s', line(7.8, 2.2, 2.2, 7.8)]],   // a spark
  DRIV: [['s', [['M', 0.8, 8.8], ['C', 0.8, 5.4, 4.4, 5.4, 4.4, 8.8]]], ['s', [['M', 3.8, 6], ['C', 3.8, 1.6, 9.2, 1.6, 9.2, 6.4]]]]   // steam puffs
};
function iconPaths(layout, names) {                       // names in slot order -> { icon0: d, icon0f: d, ... }
  const g = {}; ICON_LAYERS.forEach(l => g[l[0]] = []);
  LAYOUTS[layout].slots.forEach((s, i) => (ICONS[names[i]] || []).forEach(([kind, shape]) =>
    g['icon' + i + (kind === 'f' ? 'f' : '')].push(d(xf(shape, { x: s.ic[0], y: s.ic[1] - 11.5 })))));
  const o = {}; ICON_LAYERS.forEach(l => o[l[0]] = g[l[0]].join(' ') || 'M 0 0'); return o;
}
