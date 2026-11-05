#pragma once
#include <opxy/audio.hpp>

// toro for the OP-XY: the voice's state. The sound lives in engine.cpp.
// Copyright (c) 2026 briannhinton. MIT.
namespace toro {
struct Voice {
    opxy::u32 ph1, ph2;        // oscillator phases, 2^32 = one cycle
    opxy::i32 low, band;       // state-variable filter, Q15
    opxy::u32 rng;             // noise
    opxy::i32 f_last;          // cutoff coefficient of the last block, 0 = none yet (no sweep on a new note)
    opxy::i32 dc_x, dc_y;      // DC blocker after the folder
    opxy::u32 roll;            // per-step APHX: this note's own seed, from the track's note count
    opxy::i32 accent;          // this note's place in the accent cycle: 0, 8192 (medium) or 16384 (strong), Q15
};
struct Shared {
    opxy::u32 notes;           // notes started on this track: steps the accent cycle and seeds per-step APHX
};
}
