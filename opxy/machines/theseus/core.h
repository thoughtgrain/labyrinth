#pragma once
#include <opxy/audio.hpp>

// theseus for the OP-XY: the voice's state and the state every voice shares. The sound lives in engine.cpp.
// Copyright (c) 2026 briannhinton. MIT.
namespace theseus {
struct Voice {
    opxy::u32 ph1, ph2;        // oscillator phases
    opxy::u32 rng;             // noise
    opxy::i32 low, band;       // state-variable filter, Q15
    opxy::u32 age;             // samples since the hit
    opxy::i32 lfo;             // the hit LFO's value when this hit started, Q15
    opxy::i32 zone;            // 0 = kick .. 32767 = metal, from the pitch; -1 until the first block
    opxy::u32 serial;          // this voice's hit number (the screen reads it)
};
struct Shared {
    opxy::u32 lfo_pos;         // the hit LFO's phase: one step per note, 2^32 = one cycle
    opxy::u32 hits;            // notes started
};
}
