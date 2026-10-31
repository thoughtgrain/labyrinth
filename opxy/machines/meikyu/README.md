# meikyu, the OP-XY player

This is meikyu's shift register as an OP-XY **note player**, the stage between the keys (or the XY's sequencer) and the track's instrument where the stock Arpeggio, Maestro and Hold live. It builds against the Open-XY SDK's `firmware/native-players` branch (SDK 1.7 draft, pinned below), which is not merged yet. I wrote it because the XY has no sequencer plugins and never will through the SDK, and this is the one place a generative note source can sit.

## What it does

The OP-1 meikyu runs on its own clock. A player can't: it only answers key presses, each with a plan of up to eight timed tones that sound for as long as the key is held. So this is meikyu as a **phrase player**. The register is the OP-1's, sixteen steps with a pitch value, a weight and a flip bit, two halves A and B. Every note you play, or every note the XY's sequencer plays into the track, takes the next eight steps from the playhead as a phrase, one tone per player step, rooted on that note. Then the playhead moves on and each step that just played has a chance of being rolled again, so over a bar the pattern drifts the way the OP-1's does. A sequence of single notes becomes a line; two notes a bar becomes two phrases that answer each other.

| Encoder | Control | What it does |
|---|---|---|
| 1 | Dens | 0..100 %: a step plays when its weight is under this. The quietest quarter of the playing steps are accented. |
| 2 | Scal | minor, major, pentatonic minor, chromatic: the scale the pitch values walk up from the root |
| 3 | Crpt | 0..100 %: the chance a step is rewritten after it plays. 0 freezes the pattern, and a frozen pattern repeats every two phrases (A then B). |
| 4 | Drum | on: pitch values pick theseus's drum pads instead of scale notes (kick, a 4th and a 5th up, the octave, rim, hat, open hat, metal). The OP-1 meikyu did this automatically when theseus was loaded; a player can't see the instrument, so here it's a switch. |

**Velocity is the range.** The OP-1 had an RNG knob for how many scale degrees above the root a phrase could reach. A player only gets four encoders, so I moved it to the hand: a soft note keeps the phrase within three degrees, a hard one opens it to two octaves. The accented steps come out louder than the note you played.

What's missing against the OP-1: the clock, the LEN control (fixed at 16, two halves), the MOD lane (a player can't reach the instrument's parameters), pattern slots, and persistence of the register (it lives in the track's player state, zeroed when the track is reset; the pattern you hear after a reset is always the same starting one, from the same seed the OP-1 boots with).

## The page

The labyrinth from the OP-1 screen: the sixteen steps sit on the spiral as diamonds, lit when they would play at the current density (A outside, B inside). The eight cells on the right are the last phrase, lit where a tone sounded. The line names the last root, which half the phrase started in, and how many phrases have played. The SDK adds the stock header with the four values and the output keyboard along the bottom, which lights every pitch the phrases are sounding.

## Build and test

```sh
# in a checkout of etcetc000/open-xy at the pinned branch commit
python sdk.py test --machine ../labyrinth/opxy/machines/meikyu
python sdk.py build --machine ../labyrinth/opxy/machines/meikyu --machine ../labyrinth/opxy/machines/toro \
    --machine ../labyrinth/opxy/machines/theseus --stock-os <private>/stock-1.2.1/os.ldr --out <builds>/labyrinth-001
```

`tests/phrases.cpp` checks the phrase logic on the host: determinism of the first phrase, density bounds, scales and pads, velocity as range, the frozen two-phrase repeat, corruption, the eight-root limit, release semantics and the page strings. Players have no browser audio preview; `python3 gen_panel.py > panel.svg.fragment` regenerates the artwork.

Pinned SDK: `etcetc000/open-xy` branch `firmware/native-players` at `16c59b0` (2026-10-30). The player API (`<opxy/player.hpp>`) may still change before it lands; `machine.json` says `"sdk": "1.7"`.

## Licence

MIT, see `../../LICENSE`.
