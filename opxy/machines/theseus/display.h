#pragma once
#include <opxy/page_header.hpp>

// theseus for the OP-XY: the PHALANX screen's state. Matches resources.display in definition.json.
// Copyright (c) 2026 briannhinton. MIT.
namespace theseus {
using Frame = opxy::draw::Frame<2, 64, 119, 16, 16>;   // paths, points, nodes, texts, palette
struct Hit { opxy::i32 drum, t; };                      // drum index, ms since
struct DisplayState {
    opxy::draw::PageHeader header;     // 20
    opxy::draw::NoteTracker notes;     // 16
    opxy::u32 serial, n, seed, clock;  // 16
    Hit hit[4];                        // 32: the last four hits, newest first
    opxy::i32 side_t[3];               // 12: ms since each hand / the foot last struck
    opxy::i32 side_d[3];               // 12: the drum each struck
    opxy::i32 any_t, sway;             // 8
    opxy::i32 view, view_to, view_hold, lfo_sim;   // 16: the head shown, where it is going, frames held, a stand-in LFO
    opxy::u32 initialized;             // 4
};                                     // 136 bytes
}
