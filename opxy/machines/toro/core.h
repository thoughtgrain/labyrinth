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
};
struct Shared {};
}
