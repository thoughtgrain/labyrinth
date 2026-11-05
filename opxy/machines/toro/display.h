#pragma once
#include <opxy/page_header.hpp>

// toro for the OP-XY: the BLOCKS screen's state. Matches resources.display in definition.json.
// Copyright (c) 2026 briannhinton. MIT.
namespace toro {
using Frame = opxy::draw::Frame<0, 0, 218, 16, 10>;   // paths, points, nodes, texts, palette
struct DisplayState {
    opxy::draw::PageHeader header;     // 20 bytes
    opxy::draw::NoteTracker notes;     // 16 bytes
    opxy::u32 serial;                  // the note-start serial last seen
    opxy::i32 t_ms;                    // ms since the last note started (capped)
    opxy::u32 n;                       // notes seen, wrapped: steps the wave and picks where the next note lands
    opxy::u32 initialized;
};                                     // 36 + 16 = 52 bytes
}
