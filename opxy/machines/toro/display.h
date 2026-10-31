#pragma once
#include <opxy/page_header.hpp>

// toro for the OP-XY: the RIDER screen's state. Matches resources.display in definition.json.
// Copyright (c) 2026 briannhinton. MIT.
namespace toro {
using Frame = opxy::draw::Frame<2, 64, 64, 16, 16>;   // paths, points, nodes, texts, palette
struct DisplayState {
    opxy::draw::PageHeader header;     // 20 bytes
    opxy::draw::NoteTracker notes;     // 16 bytes
    opxy::u32 serial;                  // the note-start serial last seen
    opxy::i32 t_ms;                    // ms since the last note started
    opxy::u32 n;                       // notes seen (alternates the rock's direction)
    opxy::i32 lasso_key;               // the lasso outline's last geometry key
    opxy::u32 clock;                   // free-running ms for the spin
    opxy::u32 seed;                    // per-note seed for sparks and tears
    opxy::u32 initialized;
};
}
