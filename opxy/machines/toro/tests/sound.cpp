// toro's own sound checks, built for the host by python sdk.py test --machine <folder>.
// Sweeps every control over min / mid / max at low and high notes and checks: never above the declared peak,
// no DC lock-up, cutoff and fold audibly change the brightness, APHX is deterministic per key and gentle at a
// quarter, off at centre and re-rolled per note on the left, the accent deepens it, and the voice fits its size.
// Copyright (c) 2026 briannhinton. MIT.
#include "../engine.cpp"
#include <cstdio>
#include <cmath>
#include <cstring>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Stats { double rms, bright; int over, peak; };
static Stats play(double hz, const opxy::i32 *p, int blocks)
{
    static toro::Voice v; static toro::Shared s;
    opxy::u32 pitch[opxy::block_frames]; opxy::i32 out[opxy::block_frames];
    for (unsigned n = 0; n < opxy::block_frames; n++) pitch[n] = static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0);
    toro::initialize(v, s, 1); toro::start(v, s, 1);
    Stats st = { 0, 0, 0, 0 }; double prev = 0, sum = 0, dsum = 0; long cnt = 0;
    for (int b = 0; b < blocks; b++) {
        toro::render(v, s, 1, out, p, pitch);
        if (b < blocks / 4) continue;
        for (unsigned n = 0; n < opxy::block_frames; n++) {
            const double x = out[n] / 65536.0;
            sum += x * x; dsum += (x - prev) * (x - prev); prev = x; cnt++;
            if (out[n] > 1073741824 || out[n] < -1073741824) st.over++;
            const int a = out[n] < 0 ? -(out[n] >> 16) : out[n] >> 16;
            if (a > st.peak) st.peak = a;
        }
    }
    st.rms = std::sqrt(sum / cnt); st.bright = sum > 0 ? std::sqrt(dsum / sum) : 0;
    return st;
}
static opxy::i32 q(double f) { return static_cast<opxy::i32>(f * 2147483647.0); }
/* how far APHX pulls oscillator 1 off pitch on one note, in cents, after `before` notes on the track */
static double drift_cents(double hz, const opxy::i32 *p, opxy::u32 before)
{
    static toro::Voice v; static toro::Shared s; opxy::u32 pitch[opxy::block_frames]; opxy::i32 out[opxy::block_frames];
    const opxy::u32 i = static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0);
    for (unsigned n = 0; n < opxy::block_frames; n++) pitch[n] = i;
    s = toro::Shared(); s.notes = before;
    toro::initialize(v, s, 1); toro::start(v, s, 1); toro::render(v, s, 1, out, p, pitch);
    return 1200 * std::log2(static_cast<double>(v.ph1) / (64.0 * i));
}

int main()
{
    static_assert(sizeof(toro::Voice) == 40, "definition.json voiceBytes");
    static_assert(sizeof(toro::Shared) == 4, "definition.json sharedBytes");
    const opxy::i32 lv[3] = { 0, q(0.5), q(1.0) };
    const double notes[3] = { 41.2, 261.6, 1760.0 };
    long combos = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 3; c++) for (int d = 0; d < 3; d++)
    for (int e = 0; e < 3; e++) for (int g = 0; g < 3; g++) for (int h = 0; h < 3; h++) for (int nn = 0; nn < 3; nn++) {
        const opxy::i32 p[8] = { lv[a], lv[b], lv[c], lv[d], lv[e], lv[g], lv[h], lv[(a + h) % 3] };
        const Stats s = play(notes[nn], p, 128);
        combos++;
        CHECK(s.over == 0, "above the declared peak (%d %d %d %d / %d %d %d %d) @%.0f", a, b, c, d, e, g, h, (a + h) % 3, notes[nn]);
        if (c == 2) CHECK(s.rms > 100, "silent at open cutoff (%d %d %d %d) @%.0f: rms %.0f", a, b, c, d, notes[nn], s.rms);
    }
    opxy::i32 base[8] = { q(0.06), 0, q(0.12), q(0.12), 0, 0, 0, q(0.5) };   /* Aphx at centre: off */
    const Stats dark = play(110, base, 128); base[2] = q(1.0); const Stats open = play(110, base, 128);
    CHECK(open.bright > dark.bright * 1.5, "cutoff: dark %.3f open %.3f", dark.bright, open.bright);
    base[4] = q(1.0); base[1] = 0; const Stats nofold = play(110, base, 128);
    base[1] = q(1.0); const Stats fold = play(110, base, 128);
    CHECK(fold.bright > nofold.bright * 1.2, "fold: off %.3f on %.3f", nofold.bright, fold.bright);
    /* DC: a held note averages near zero */
    {
        const opxy::i32 z[8] = { q(0.12), q(0.6), q(1.0), q(0.6), q(0.5), 0, q(0.1), q(0.5) };
        static toro::Voice v; static toro::Shared s; opxy::u32 pitch[64]; opxy::i32 out[64];
        for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(261.6 / 44100.0 * 4294967296.0);
        toro::initialize(v, s, 1); toro::start(v, s, 1);
        double mean = 0; long cnt = 0;
        for (int b = 0; b < 800; b++) { toro::render(v, s, 1, out, z, pitch); if (b >= 200) for (int n = 0; n < 64; n++) { mean += out[n] / 65536.0; cnt++; } }
        CHECK(std::fabs(mean / cnt) < 800, "DC offset %.0f", mean / cnt);
    }
    /* APHX (right of centre, per key): the same key at the same place in the accent cycle renders the same samples;
     * drift is a few cents at 25 % and large at 100 % */
    {
        const opxy::i32 ap[8] = { q(0.12), q(0.37), q(0.67), q(0.27), q(0.18), 0, q(0.3), q(1.0) };
        static opxy::i32 A1[64 * 40], A2[64 * 40];
        for (int rep = 0; rep < 2; rep++) {
            static toro::Voice v; static toro::Shared s; opxy::u32 pitch[64];
            for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(277.18 / 44100.0 * 4294967296.0);
            s = toro::Shared(); toro::initialize(v, s, 1); toro::start(v, s, 1);
            for (int b = 0; b < 40; b++) toro::render(v, s, 1, (rep ? A2 : A1) + b * 64, ap, pitch);
        }
        CHECK(std::memcmp(A1, A2, sizeof A1) == 0, "APHX: the same key must sound the same every time");
        double worst_lo = 0, worst_hi = 0; int differ = 0;
        for (int key = 0; key < 24; key++) {
            const double hz = 27.5 * std::pow(2, key / 12.0);
            const opxy::i32 lo[8] = { 0, 0, q(1.0), 0, 0, 0, 0, q(0.625) }, hi[8] = { 0, 0, q(1.0), 0, 0, 0, 0, q(1.0) };
            const double cents[2] = { drift_cents(hz, lo, 1), drift_cents(hz, hi, 1) };   /* a weak note: no accent */
            if (std::fabs(cents[0]) > worst_lo) worst_lo = std::fabs(cents[0]);
            if (std::fabs(cents[1]) > worst_hi) worst_hi = std::fabs(cents[1]);
            differ += std::fabs(cents[1]) > 30;
        }
        CHECK(worst_lo > 0.5 && worst_lo < 25, "APHX 25 %%: a few cents, worst %.1f", worst_lo);
        CHECK(worst_hi > 60 && differ >= 8, "APHX 100 %%: keys thrown off pitch (worst %.0f cents, %d of 24 keys moved)", worst_hi, differ);
        std::printf("APHX drift: worst %.1f cents at 25 %%, %.0f cents at 100 %% (%d of 24 keys > 30)\n", worst_lo, worst_hi, differ);
    }
    /* APHX at centre is off: no drift on any key, accent or not */
    {
        const opxy::i32 c[8] = { 0, 0, q(1.0), 0, 0, 0, 0, q(0.5) };
        double worst = 0;
        for (int key = 0; key < 24; key++) for (int before = 0; before < 4; before++) {
            const double cc = std::fabs(drift_cents(27.5 * std::pow(2, key / 12.0), c, static_cast<opxy::u32>(before)));
            if (cc > worst) worst = cc;
        }
        CHECK(worst < 0.01, "APHX centre: should not drift, worst %.3f cents", worst);
    }
    /* APHX left of centre (per step): one key re-rolls on every note, and the same note count rolls the same way */
    {
        const opxy::i32 st[8] = { 0, 0, q(1.0), 0, 0, 0, 0, 0 };
        double c1[8], c2[8]; int moved = 0, distinct = 0;
        for (int k = 0; k < 8; k++) { c1[k] = drift_cents(220.0, st, static_cast<opxy::u32>(4 * k + 1)); c2[k] = drift_cents(220.0, st, static_cast<opxy::u32>(4 * k + 1)); }
        for (int k = 0; k < 8; k++) {
            CHECK(c1[k] == c2[k], "APHX per step: note %d should roll the same way for the same count", k);
            moved += std::fabs(c1[k]) > 30;
            bool fresh = true;
            for (int j = 0; j < k; j++) if (std::fabs(c1[j] - c1[k]) < 1) fresh = false;
            distinct += fresh;
        }
        CHECK(moved >= 3 && distinct >= 7, "APHX per step: the same key should re-roll (%d of 8 moved > 30 cents, %d distinct)", moved, distinct);
        std::printf("APHX per step: %d of 8 notes on one key moved > 30 cents, %d distinct rolls\n", moved, distinct);
    }
    /* the accent: a strong note drifts further than a weak one on the same key */
    {
        const opxy::i32 half[8] = { 0, 0, q(1.0), 0, 0, 0, 0, q(0.75) };
        int deeper = 0, keys = 0;
        for (int key = 0; key < 24; key++) {
            const double hz = 27.5 * std::pow(2, key / 12.0);
            const double strong = std::fabs(drift_cents(hz, half, 0)), weak = std::fabs(drift_cents(hz, half, 1));
            if (weak < 0.5) continue;
            keys++; deeper += strong > weak * 1.1;
        }
        CHECK(keys >= 16 && deeper >= keys - 1, "accent: strong notes should drift further (%d of %d keys)", deeper, keys);
        std::printf("accent: strong drifts further on %d of %d keys\n", deeper, keys);
    }
    std::printf("%ld control/note combinations; dark %.3f open %.3f, fold off %.3f on %.3f\n", combos, dark.bright, open.bright, nofold.bright, fold.bright);
    std::printf(fails ? "%d FAILED\n" : "all toro checks passed\n", fails);
    return fails != 0;
}
