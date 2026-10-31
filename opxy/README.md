# toro and theseus for the OP-XY

This folder is my port of two of the three labyrinth plugins to the teenage engineering OP-XY, built on the [Open-XY SDK](https://github.com/etcetc000/open-xy). The OP-1 versions in the folders above are untouched; this is a separate tree with its own licence (MIT, like the SDK and most XY machines).

- **toro**: the two-oscillator wavefolder synth, with APHX.
- **theseus**: the DFAM-style percussion voice.
- **meikyu**: the shift register as a note player (phrase mode), against the SDK's unmerged `firmware/native-players` branch. See [What about meikyu?](#what-about-meikyu).

![toro on the XY: the rider at rest](docs/toro_rest.png)

![toro on the XY: thrown at full Fold, the lasso out](docs/toro_fold.png)

![theseus on the XY: front for the kick](docs/theseus_kick.png)

![theseus on the XY: the helmet turned to the hats, and to the cymbal](docs/theseus_hat.png)

![the meikyu player page: the register on the labyrinth, the last phrase on the right](docs/meikyu_page.png)

## What changed in the port

The OP-XY and the OP-1 are different instruments, and the SDKs reflect that. The sound code moved over almost as is; the screens had to be rebuilt.

| | OP-1 (kennel) | OP-XY (Open-XY) |
|---|---|---|
| Language | C | C++11, freestanding |
| Block | 128 samples | 64 samples |
| Controls | 4 knobs, 3 second-page words | 4 encoders with up to 4 alternates, all saved: 8 slots |
| Values | 0..32767 | Q31 in, Q31 out at half scale |
| Velocity | the engine reads it | not given to engines; the XY applies it after |
| Voice memory | 32 bytes | whatever it needs |
| Screen | 320 x 160, draw any lines you like each frame | 480 x 220, retained: you author SVG parts, each frame only moves, turns, scales, recolours or hides them |
| Colour | the four encoder colours | greyscale |

### Sound

Both engines keep their integer DSP byte for byte where the block size allows. The 0..32767 control range is taken from the top 15 bits of each Q31 word, so the knob curves are the OP-1's. Envelope ramps are per 64 samples instead of 128.

**toro** gets the split it never had room for on the OP-1: Drive and Aphx are separate slots on page 2. Velocity is gone from the engine, so APHX's accent-deepening is gone with it (the XY's velocity reaches the track level instead).

**theseus** keeps the per-hit LFO. Its phase lives in the machine's `Shared` state so every voice steps the same LFO, as on the OP-1. The two envelope divides per block became a reciprocal (the Blackfin+ has no divide). The eighth slot is new: **Tune**, -24 to +24 semitones, which shifts which drum a note picks without changing the note. On the XY the sequencer plays fixed pitches, so this is how you move a whole pattern from toms to hats. Velocity accents are gone from the engine; the XY's own velocity path stands in.

On the XY's style guide: it says engine pages should not hold LFOs, because the XY has its own. I kept theseus's LFO anyway, because it steps once per hit, which no stock LFO can do, and it is the thing that makes a pattern breathe. If the catalog review pushes back, the LFO and Rate slots are the ones to drop.

### Screens

The OP-1 screens drew hundreds of fresh lines per frame. The XY draws authored SVG and lets code move the parts, with a budget of 256 parts, 4 reshapable outlines and 16 greys. So each screen is now a `gen_panel.py` that writes the artwork (`panel.svg.fragment`) with every moving part as a separate element around its own pivot, and a `display.cpp` that places them. The geometry is lifted straight from the OP-1 sources (the same rig units, the same Bezier outlines for the bull's plate and the helmet's five heads), and anything the OP-1 drew as a chain of lines (the horns, the cowboy's legs and lasso arm, the drummer's sticks) is a chain of nodes here, each placed at the end of the one before it and turned. The composition and the per-control shapes are the OP-1's:

| toro | | theseus | |
|---|---|---|---|
| Spread | the horns grow and swing out; the hat brim with them | Decay | ripple rings from the struck head |
| Fold | the rig rocks, the cowboy is thrown, his legs kick and his spurs spin, a pulse runs the hose | Punch | the kick beater winds back; a diamond on impact |
| Cutoff | the eye slits open, his eyes with them; a note lights the slits | Cutoff | the eyes burn; rays on strong hits |
| Res | the nose ring swings; the lasso rises and spins | FM | the crest grows, goes jagged and shakes |
| Shape | the horn tips curl inward | Noise | sizzle specks |
| Noise | sparks off the turntable | LFO | the head slides, with motion arcs |
| Drive | steam from the vents | Rate | how fast it slides |
| Aphx | glitch slices tear across the rig | Tune | moves which drum the sticks land on |

Greyscale was less of a loss than I expected. The OP-1 screens already gave every control its own shape so colour was never the only cue; on the XY the shapes do all the work, with grey levels for lit against unlit.

The look is a vintage western print, the kind of ink-on-cream label art with heavy contours, solid fills and hatching for shade, inverted for the XY's black screen: light-filled shapes (`#C9C9C9` paper, `#E2E2E2` cream) with ink (`#0A0A0A`) detail drawn on top of them, hatching as short ink strokes on the shaded sides, and starburst ornaments in the corners. That inversion is what makes it work on an OLED: a black line around a cream shape would vanish against the black background, so the contour is the fill's own edge and the ink goes inside.

Two XY-specific things worth knowing, plus the device's own drawing rules:

- **The screen doesn't know the pitch.** The XY tells a display how many notes are held and counts note starts; it doesn't say which note. On the OP-1 the engine told the screen which drum zone a hit landed in. Here theseus's screen walks a stand-in pattern (kick-heavy, like a DRM pattern, seeded by the note count) to decide which drum the sticks hit. The sound follows the real pitch; the drawing follows a plausible one. The head still turns toward the drum it draws.
- **Paint comes from a palette.** A part painted "fill and stroke" takes both from one palette entry, so the palette has two halves: entries 0..7 a near-black fill with an outline from dim to white, 8..15 solid greys. Most parts are painted `keep` now and carry their own greys in the artwork; the palette is for the few that change (the eye highlights, the lasso, the ripple rings).
- **The device is stricter than the preview.** Open-XY's PR 260 ("Drawing on the device" in `docs/display.md`) lists what the hardware does with artwork: every path must be one subpath of at most 64 points (compound paths stopped the device at launch), a turn within 2.5° of straight back draws a streak, points closer than 1/8 px merge, and only an authored fill is ever filled. So every figure is a **group** of single-subpath parts sharing a pivot: `gen_panel.py` assigns the node indices and writes `nodes.h`, and `display.cpp` moves a group with one call. toro is 125 nodes, theseus 119. `tools/panel_lint.py` checks all three panels against those rules; run it after regenerating artwork.

## Build and try

You need the Open-XY SDK checked out beside this repository, with its preview tools (Python 3.11+, NumPy 2.3.5, Node 22.12+, clang with wasm32). Then, from the SDK folder:

```sh
python sdk.py test --machine ../labyrinth/opxy/machines/toro
python sdk.py test --machine ../labyrinth/opxy/machines/theseus
python sdk.py preview --machine ../labyrinth/opxy/machines/toro --out local/toro
python tools/serve_harness.py --preview local/toro      # then open http://127.0.0.1:8917/
```

`test` runs each machine's own sound checks (`tests/sound.cpp`: the same sweeps as the OP-1 tests, ported), builds the browser preview, checks every drawing frame against the SDK's bounds, and prints a host-timed CPU estimate. To build firmware you need the pinned Blackfin+ toolchain and your own `os.ldr`; the SDK's `docs/build.md` covers that.

If you change the artwork, regenerate it: `python3 gen_panel.py` in toro's or theseus's folder (it writes `panel.svg.fragment`, `nodes.h` and the node count into `definition.json`), `python3 gen_panel.py > panel.svg.fragment` in meikyu's; then `python3 tools/panel_lint.py machines/*/panel.svg.fragment`.

## Status

| Check | Status |
|---|---|
| toro sound test (6561 control/note combinations, APHX determinism and drift) | passed |
| theseus sound test (5832 control/pad combinations, zones, LFO rate, Tune) | passed |
| `sdk.py test` on both: preview ABI, memory, peak bounds, every frame valid | passed |
| CPU estimate (host-timed, not the device) | toro about 9–11 k, theseus about 13–15 k instructions per voice block at the heaviest setting; the aim is 15 k |
| Screens in the SDK's browser workbench | checked |
| meikyu player: host test against the `firmware/native-players` branch | passed; not yet heard on hardware |
| Firmware build and the OP-XY itself | **not done**: no Blackfin+ toolchain or XY here. The catalog also needs a hardware CPU measurement (`docs/profiling.md` in the SDK) |

theseus is the one to measure first: at its heaviest setting (full FM and noise, a long decay) the estimate sits at the aim. If the device says it is over, the noise mix and the FM index are the two things to make cheaper.

## What about meikyu?

The OP-XY has no sequencer plugins. The SDK offers synths, sample engines and effects, plus "extensions" that observe firmware events, and the two events a sequencer would need (a pattern step playing, notes reaching a track) are listed as not exposed. The right home for meikyu on the XY is a **player**, the XY's own note-effect stage (arpeggio, maestro, hold).

The SDK's author has that in progress: the `firmware/native-players` branch of `etcetc000/open-xy` (SDK 1.7 draft, six hooks into the stock Maestro object, a `<opxy/player.hpp>` API) with a Chords player as the example in `open-xy-core-machines`. A player answers key presses with a plan of up to eight timed tones; it has no clock and can't sound a note on its own. So `machines/meikyu/` is meikyu as a **phrase player**: every note, live or sequenced, plays the next eight steps of the self-corrupting register, rooted on that note, and the pattern drifts across the bar. Its [README](machines/meikyu/README.md) has the controls and what's missing against the OP-1. It passes the branch's `sdk.py test` on the host and is waiting on a firmware build to be heard; the branch is unmerged, so the API may still move under it.

## Licence

MIT, see `LICENSE` in this folder. The OP-1 plugins in the folders above stay GPL-3.0-or-later with the OP-1 firmware permission; this tree is a separate work that shares their design and DSP, which I own.
