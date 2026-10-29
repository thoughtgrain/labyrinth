# Design prototypes

These are the browser prototypes I designed both screens in, before porting them to C. They drew the canvas boards: `thread.js` (meikyu), `bull.js` (toro and the cowboy), `icons.js`, `layouts.js` (readout slots), with `check2.js` as the layout-rule check. They use floats and SVG paths, so they're reference only; the plugins don't use them. Where the C differs (page-2 values always act on the drawing, pattern pips, the MOD page, FLIP marks, per-knob page 2 on toro), the C is the one that matches the plugins.
