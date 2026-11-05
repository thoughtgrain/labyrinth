#pragma once
#include <opxy/page_header.hpp>

// theseus for the OP-XY: the TOWER screen's state. Matches resources.display in definition.json (gen_panel.py reads
// the size from the stateBytes line below).
// Copyright (c) 2026 briannhinton. MIT.
namespace theseus {
using Frame = opxy::draw::Frame<1, 64, 232, 16, 16>;   // paths, points, nodes, texts, palette
struct DisplayState {
    opxy::draw::PageHeader header;     // 20
    opxy::draw::NoteTracker notes;     // 16
    opxy::u32 serial, n, initialized;  // 12: the note serial last seen, hits seen, set up
    opxy::i32 hit_t[5];                // 20: ms since each tier (kick, floor, snare, rack tom, hat) was last hit
    opxy::i32 amp[5];                  // 20: how high that hit jumps, Q10 px
    opxy::i32 zone[5];                 // 20: the zone it played, Q10 (high drums ring shorter, spark more)
    opxy::u32 hit_n[5];                // 20: its hit number, which seeds its sparks
    opxy::i32 rock_t[2], rock_side[2]; // 16: the last two hat hits, ms since and which way they rock
    opxy::u32 slice_key;               // 4: the floor slice's geometry key
};                                     // stateBytes 148
}
