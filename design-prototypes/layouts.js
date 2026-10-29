// Readout layouts from the kit: names XS (13.3 tall), values S (20 tall) with an XS unit on the same baseline.
// Every slot is fixed: values never move with the artwork. ART is the region the drawing must stay inside.
const XS = 18.3, SZ = 27.4, ADV = 0.6;                   // font sizes giving 13.3 / 20 unit cap heights; monospace advance
const LAYOUTS = {
  A: { art: [8, 8, 185, 144], slots: [29, 62, 95, 128].map(y => ({ n: [197, y, 'start'], v: [311, y, 'end'] })) },
  AL: { art: [135, 8, 312, 144], slots: [29, 62, 95, 128].map(y => ({ n: [9, y, 'start'], v: [123, y, 'end'] })) },
  B: { art: [8, 8, 312, 86], slots: [9, 89, 169, 249].map(x => ({ n: [x, 112, 'start'], v: [x, 138, 'start'] })) },
  C: { art: [87, 8, 233, 144], slots: [{ n: [9, 22, 'start'], v: [9, 46, 'start'] }, { n: [9, 114, 'start'], v: [9, 138, 'start'] },
                                       { n: [311, 22, 'end'], v: [311, 46, 'end'] }, { n: [311, 114, 'end'], v: [311, 138, 'end'] }] },
  D: { art: [8, 8, 177, 144], box: [8, 41, 74, 107].map(y => [189, y, 122, 30]), slots: [33, 66, 99, 132].map(y => ({ n: [194, y, 'start'], v: [306, y, 'end'] })) },
  D2: { art: [8, 8, 196, 144], box: [[199, 42], [257, 42], [199, 92], [257, 92]].map(([x, y]) => [x, y, 55, 46]),
        slots: [[199, 42], [257, 42], [199, 92], [257, 92]].map(([x, y]) => ({ n: [x + 5, y + 18, 'start'], v: [x + 5, y + 42, 'start'] })) },
  DL: { art: [142, 8, 312, 144], box: [8, 41, 74, 107].map(y => [8, y, 122, 30]), slots: [33, 66, 99, 132].map(y => ({ n: [13, y, 'start'], v: [125, y, 'end'] })) },
  DF: { art: [8, 8, 312, 80], box: [0, 1, 2, 3].map(i => [8 + i * 76, 92, 74, 46]), slots: [0, 1, 2, 3].map(i => ({ n: [13 + i * 76, 110, 'start'], v: [13 + i * 76, 134, 'start'] })) },
  DC: { art: [94, 8, 226, 144], box: [[8, 8], [8, 92], [238, 8], [238, 92]].map(([x, y]) => [x, y, 74, 46]), slots: [[8, 8], [8, 92], [238, 8], [238, 92]].map(([x, y]) => ({ n: [x + 5, y + 18, 'start'], v: [x + 5, y + 42, 'start'] })) },
  D2L: { art: [129, 8, 312, 144], box: [[8, 42], [66, 42], [8, 92], [66, 92]].map(([x, y]) => [x, y, 55, 46]), slots: [[8, 42], [66, 42], [8, 92], [66, 92]].map(([x, y]) => ({ n: [x + 5, y + 18, 'start'], v: [x + 5, y + 42, 'start'] })) },
  // bespoke corner placements: each readout sits in the corner nearest the part it drives; art may fill the rest
  // icons sit on the outer edge of each corner, the name beside them: 10 x 10, bottom aligned with the name's baseline
  MK: { art: [8, 8, 312, 144], slots: [{ n: [23, 22, 'start'], v: [9, 46, 'start'], ic: [9, 22] }, { n: [297, 22, 'end'], v: [311, 46, 'end'], ic: [301, 22] },
                                       { n: [23, 114, 'start'], v: [9, 138, 'start'], ic: [9, 114] }, { n: [297, 114, 'end'], v: [311, 138, 'end'], ic: [301, 114] }] },
  TK: { art: [8, 8, 312, 144], slots: [{ n: [23, 22, 'start'], v: [9, 46, 'start'], ic: [9, 22] }, { n: [297, 114, 'end'], v: [311, 138, 'end'], ic: [301, 114] },
                                       { n: [23, 114, 'start'], v: [9, 138, 'start'], ic: [9, 114] }, { n: [297, 22, 'end'], v: [311, 46, 'end'], ic: [301, 22] }] },
  E: { art: [8, 58, 312, 144], slots: [9, 89, 169, 249].map(x => ({ n: [x, 22, 'start'], v: [x, 46, 'start'] })) }
};
function textBox(x, y, anchor, w, cap) { const x0 = anchor === 'end' ? x - w : x; return [x0, y - cap, x0 + w, y + 1]; }
function slotBoxes(layout, name, val, unit) {            // ink boxes for one slot's name and value group
  return LAYOUTS[layout].slots.map((s, i) => [
    textBox(s.n[0], s.n[1], s.n[2], name[i].length * XS * ADV, 13.4),
    textBox(s.v[0], s.v[1], s.v[2], val[i].length * SZ * ADV + unit[i].length * XS * ADV, 20.1)]);
}
