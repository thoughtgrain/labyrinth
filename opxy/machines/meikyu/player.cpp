/* meikyu for the OP-XY: a note player built on the OP-1 sequencer's shift register.
 *
 * The OP-1 meikyu is a free-running generative sequencer. The XY's player stage has no clock of its own: a player
 * only answers key presses, each with a plan of up to eight timed tones that sound while the key is held. So this
 * is meikyu as a phrase player. The register is the OP-1's: sixteen steps, each with a pitch value (0..15), a
 * weight (0..99) and a flip bit. A step plays when its weight is under the density. Every press, live or from the
 * XY's sequencer, takes the next eight steps from the playhead as a phrase, one tone per player step, rooted on the
 * note played; then the playhead moves on, and each step that just played has a chance (Corrupt) of being rolled
 * again. Over a sequence the pattern drifts the way the OP-1's does, and a bar of single notes becomes a line.
 *
 *   Dens   0..100 %: how many steps play (weight < density; the quietest quarter of those are accented)
 *   Scal   minor, major, pentatonic minor, chromatic: the scale the pitch values walk, rooted on the note
 *   Crpt   0..100 %: the chance a step is rewritten after it plays; 0 freezes the pattern
 *   Drum   on: pitch values pick drum pads instead (the OP-1's DRM style for theseus: kick a 4th, a 5th, octave,
 *          rim, hat, open hat, metal); the player cannot see the track's instrument, so this is a switch
 *
 * Velocity sets the range: a soft note keeps the phrase within a few scale degrees, a hard one opens it up to two
 * octaves (the OP-1's RNG knob, played rather than turned). Held notes are tracked so a release only drops its own
 * phrase; the register itself persists until the project does something about it (it is zeroed with the track).
 * Copyright (c) 2026 briannhinton. MIT. */
#include <opxy/player.hpp>

namespace meikyu {
using opxy::u8;
using opxy::u16;
using opxy::u32;
using opxy::i32;
using opxy::player::Controls;
using opxy::player::Display;
using opxy::player::Plan;

const u32 kSteps = 16;              // the register: two eight-step halves
const u32 kPhrase = 8;              // steps per press, the ABI's tone limit
const u32 kRoots = 8;

struct Root { u8 note; u8 mask; };  // note + 1 (0 = empty); bit k: phrase tone k sounded

struct State {
    u8 initialized;
    u8 head;                        // the playhead: the step the next press starts from
    u8 val[kSteps];                 // pitch value 0..15
    u8 wt[kSteps];                  // weight 0..99
    u16 flip;                       // bit i: step i's play decision inverted (kept for the OP-1's data shape)
    u16 rewrote;                    // bit i: step i was rewritten by Corrupt on its last pass (for the page)
    u32 rng;
    u8 last_root;                   // note + 1 of the last phrase, for the page
    u8 last_start;                  // the step the last phrase started at
    u8 last_mask;                   // which of its eight tones sounded
    u8 phrases;                     // phrases played, for the page
    Root roots[kRoots];
};

namespace {
/* bit n = n semitones above the root: minor, major, pentatonic minor, chromatic (the OP-1's masks) */
const u16 kMasks[4] = { 0x5AD, 0xAB5, 0x4A9, 0xFFF };
const char* const kScaleNames[4] = { "min", "maj", "pent", "chrom" };
/* DRM pads: the held note is the kick, a 4th and 5th up the toms, the octave the snare, then rim, hat, open hat, metal */
const u8 kPadSemis[8] = { 0, 5, 7, 12, 17, 24, 31, 36 };

u32 rand32(State& s)
{
    u32 x = s.rng ? s.rng : 0x6D617A65u;            /* xorshift32; never zero */
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s.rng = x;
    return x;
}
u32 pct(State& s) { return (rand32(s) >> 8) % 100u; }
void roll(State& s, u32 i)
{
    s.val[i] = static_cast<u8>((rand32(s) >> 12) & 15);
    s.wt[i] = static_cast<u8>(pct(s));
    s.flip &= static_cast<u16>(~(1u << i));
}
void init(State& s)
{
    s.rng = 0x1AB71217u;                            /* the OP-1's synth bank seed: the first phrase is always the same */
    for (u32 i = 0; i < kSteps; ++i) roll(s, i);
    s.head = 0; s.flip = 0; s.rewrote = 0;
    s.initialized = 1;
}

u32 steps(i32 value, u32 count)                     /* Q31 0..1 -> 0..count-1 */
{
    const u32 v = value < 0 ? 0u : static_cast<u32>(value) >> 16;
    const u32 step = (v * count) >> 15;
    return step < count ? step : count - 1;
}
u32 percent(i32 value) { return steps(value, 101); }
u32 density_of(const Controls& c) { return (steps(c.value[0], 64) * 100 + 31) / 63; }   /* encoder 1 moves in 64 steps */
u32 scale_of(const Controls& c) { return steps(c.value[1], 4); }
u32 corrupt_of(const Controls& c) { return percent(c.value[2]); }
bool drum_of(const Controls& c) { return steps(c.value[3], 2) != 0; }

bool plays(const State& s, u32 i, u32 density)
{
    const bool on = s.wt[i] < density;
    return ((s.flip >> i) & 1) ? !on : on;
}
bool accent(const State& s, u32 i, u32 density) { return s.wt[i] < density / 4; }

/* the i-th scale note at or above the root: snap up into the scale, then walk deg scale notes (the OP-1's walk) */
i32 scale_note(i32 root, u16 mask, i32 deg)
{
    const i32 r = root % 12;
    i32 n = root;
    for (i32 k = 0; k < 12 && !((mask >> ((n - r + 120) % 12)) & 1); ++k) ++n;
    while (deg > 0) { ++n; if ((mask >> ((n - r + 120) % 12)) & 1) --deg; }
    return n;
}
/* velocity -> range in scale degrees: 3 at the softest, 15 at the hardest (the OP-1's RNG 0..15) */
u32 range_of(u32 velocity) { return 3 + (velocity * 13) / 128; }

struct Text {
    char* out; u32 size, length;
    Text& operator<<(const char* s) { while (*s && length + 1 < size) out[length++] = *s++; out[length] = 0; return *this; }
    Text& operator<<(i32 n)
    {
        char digits[12]; u32 count = 0, v = n < 0 ? static_cast<u32>(-n) : static_cast<u32>(n);
        do { digits[count++] = static_cast<char>('0' + v % 10); v /= 10; } while (v);
        if (n < 0) *this << "-";
        while (count) { const char c[2] = { digits[--count], 0 }; *this << c; }
        return *this;
    }
};
template <u32 N> Text text(char (&out)[N]) { out[0] = 0; return Text{ out, N, 0 }; }
const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

bool press(State& s, const Controls& c, u32 note, u32 velocity, Plan& plan)
{
    plan.count = 0;
    if (note > 127 || velocity > 127) return true;
    if (!s.initialized) init(s);
    /* a held note pressed again (the sequencer retriggering) restarts its phrase; otherwise take a free slot */
    Root* slot = 0;
    for (u32 r = 0; r < kRoots; ++r) if (s.roots[r].note == note + 1) slot = &s.roots[r];
    if (!slot) for (u32 r = 0; r < kRoots; ++r) if (!s.roots[r].note) { slot = &s.roots[r]; break; }
    if (!slot) return true;                                     /* eight phrases already sounding */

    const u32 density = density_of(c), corrupt = corrupt_of(c);
    const u16 mask = kMasks[scale_of(c)];
    const bool drum = drum_of(c);
    const u32 range = range_of(velocity);
    const u32 start = s.head;
    u8 sounded = 0;
    for (u32 k = 0; k < kPhrase; ++k) {
        const u32 i = (start + k) % kSteps;
        s.rewrote &= static_cast<u16>(~(1u << i));              /* a new pass over step i: its old mark goes */
        if (plays(s, i, density)) {
            const i32 deg = static_cast<i32>((s.val[i] * range) / 15);          /* 0..range */
            const i32 pitch = drum ? static_cast<i32>(note) + kPadSemis[(deg >> 1) & 7] : scale_note(static_cast<i32>(note), mask, deg);
            if (pitch >= 0 && pitch <= 127) {
                opxy::player::Tone& tone = plan.tones[plan.count++];
                tone.pitch = static_cast<u8>(pitch);
                tone.velocity = static_cast<u8>(accent(s, i, density) ? (velocity > 100 ? 127 : velocity + 27) : velocity);
                tone.delay = static_cast<u16>(k);                               /* one player step per register step */
                sounded |= static_cast<u8>(1u << k);
            }
        }
        if (corrupt && pct(s) < corrupt) {                      /* corrupt the step just passed, as the OP-1 does after it plays */
            roll(s, i);
            s.rewrote |= static_cast<u16>(1u << i);
        }
    }
    s.head = static_cast<u8>((start + kPhrase) % kSteps);
    s.phrases++;
    s.last_root = static_cast<u8>(note + 1); s.last_start = static_cast<u8>(start); s.last_mask = sounded;
    /* an empty phrase is a rest: the key is taken (so release knows it) but nothing sounds */
    slot->note = static_cast<u8>(note + 1);
    slot->mask = sounded;
    return true;
}

bool release(State& s, u32 note)
{
    if (note > 127) return true;
    bool found = false, sounding = false;
    for (u32 r = 0; r < kRoots; ++r)
        if (s.roots[r].note == note + 1) { found = true; sounding = sounding || s.roots[r].mask != 0; s.roots[r] = Root{}; }
    return found && !sounding;                                  /* a silent phrase has no stock chord to release; an unknown note is stock's */
}

void reset(State& s)
{
    for (u32 r = 0; r < kRoots; ++r) s.roots[r] = Root{};      /* the register and its playhead survive a stop */
}

/* the page: dots 0..15 are the register's steps (lit when they play at the current density), dots 16..23 the eight
 * tones of the last phrase (lit when they sounded); the text names the last root and the step the phrase began at */
void display(const State& s, const Controls& c, Display& d)
{
    const u32 density = density_of(c);
    d.lit = 0;
    if (s.initialized) {
        for (u32 i = 0; i < kSteps; ++i) if (plays(s, i, density)) d.lit |= 1u << i;
        for (u32 k = 0; k < kPhrase; ++k) if ((s.last_mask >> k) & 1) d.lit |= 1u << (16 + k);
    }
    Text line = text(d.text);
    if (s.last_root) line << kNoteNames[(s.last_root - 1) % 12] << " " << (s.last_start < 8 ? "A" : "B") << " phrase " << static_cast<i32>(s.phrases);
    else line << "meikyu";
    Text dens = text(d.values[0]); dens << static_cast<i32>(density);
    Text scal = text(d.values[1]); scal << kScaleNames[scale_of(c)];
    Text crpt = text(d.values[2]); crpt << static_cast<i32>(corrupt_of(c));
    Text drum = text(d.values[3]); drum << (drum_of(c) ? "on" : "off");
}
}
