#pragma once
#include <opxy/page_header.hpp>

// toro for the OP-XY: the TAURUS screen's state. Matches resources.display in definition.json.
// Copyright (c) 2026 briannhinton. MIT.
namespace toro {
using Frame = opxy::draw::Frame<2, 64, 68, 16, 16>;   // paths, points, nodes, texts, palette
struct DisplayState {
    opxy::draw::PageHeader header;     // 20 bytes
    opxy::draw::NoteTracker notes;     // 16 bytes
    opxy::u32 serial;                  // the note-start serial last seen
    opxy::i32 t_ms;                    // ms since the last note started
    opxy::u32 n;                       // notes seen (alternates the sky's turn)
    opxy::i32 lasso_key;               // the Pleiades' angle, tenths of a degree (the name is kept from the rider screen's layout)
    opxy::u32 clock;                   // free-running ms
    opxy::u32 seed;                    // per-note seed for comets and twinkle
    opxy::u32 initialized;             // 64 so far
    opxy::i32 comet_t[4];              // ms into each comet's flight, -1 when none
    opxy::i32 comet_x[4], comet_y[4];  // the flight's centre, pixels
    opxy::i32 comet_a[4];              // heading, tenths of a degree
    opxy::i32 comet_l[4];              // flight time, ms
    opxy::i32 idle_ms;                 // ms since the last note, for the idle comet
    opxy::i32 spin;                    // the Pleiades' spin rate, tenths of a degree per second
    opxy::i32 sky;                     // the sky's turn about Aldebaran, tenths of a degree
};                                     // 64 + 80 + 12 = 156 bytes
}
