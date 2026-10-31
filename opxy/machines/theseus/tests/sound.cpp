// theseus's own sound checks, built for the host by python sdk.py test --machine <folder>.
// Sweeps the seven continuous controls over min / mid / max at the eight DRM pad pitches and checks: never above
// the declared peak, every hit audible and then decaying to silence, kick longer and darker than hat, Punch sweeps
// the pitch, Cutoff / FM / Noise change the brightness, the hit LFO steps once per hit at the set rate, and Tune
// moves the zone. Copyright (c) 2026 briannhinton. MIT.
#include "../engine.cpp"
#include <cstdio>
#include <cmath>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static opxy::i32 q(double f) { return static_cast<opxy::i32>(f * 2147483647.0); }
static double pad_hz(int pad) { static const int off[8] = { 0, 5, 7, 12, 17, 24, 31, 36 }; return 65.41 * std::pow(2, off[pad] / 12.0); }

struct Hit { double head, tail, bright; int over, peak; long len; double zc1, zc2; };
static theseus::Shared shared;
static Hit hit(double hz, const opxy::i32 *p, int blocks)
{
    static theseus::Voice v;
    opxy::u32 pitch[64]; opxy::i32 out[64];
    for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0);
    theseus::initialize(v, shared, 1); theseus::start(v, shared, 1);
    Hit h = { 0, 0, 0, 0, 0, 0, 0, 0 }; double prev = 0, s2 = 0, d2 = 0, head = 0, tail = 0; long nh = 0, nt = 0; int last = 0;
    for (int b = 0; b < blocks; b++) {
        theseus::render(v, shared, 1, out, p, pitch);
        for (int n = 0; n < 64; n++) {
            const double x = out[n] / 65536.0; const long t = static_cast<long>(b) * 64 + n;
            s2 += x * x; d2 += (x - prev) * (x - prev); prev = x;
            if (t < 4410) { head += x * x; nh++; } else if (b >= blocks - 16) { tail += x * x; nt++; }
            const int sg = x > 64 ? 1 : (x < -64 ? -1 : 0);
            if (sg && last && sg != last) { if (t < 441) h.zc1++; else if (t >= 2205 && t < 2646) h.zc2++; }
            if (sg) last = sg;
            if (out[n] > 1073741824 || out[n] < -1073741824) h.over++;
            const int a = static_cast<int>(std::fabs(x)); if (a > h.peak) h.peak = a;
            if (a > 300) h.len = t;
        }
    }
    h.head = std::sqrt(head / nh); h.tail = nt ? std::sqrt(tail / nt) : 0; h.bright = s2 > 0 ? std::sqrt(d2 / s2) : 0;
    return h;
}

int main()
{
    static_assert(sizeof(theseus::Voice) == 36 && sizeof(theseus::Shared) == 8, "definition.json voiceBytes / sharedBytes");
    const opxy::i32 lv[3] = { 0, q(0.5), q(1.0) };
    long combos = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 3; c++) for (int d = 0; d < 3; d++)
    for (int e = 0; e < 3; e++) for (int g = 0; g < 3; g++) for (int pad = 0; pad < 8; pad++) {
        const opxy::i32 p[8] = { lv[a], lv[b], lv[c], lv[d], lv[e], lv[g], lv[(a + c) % 3], q(0.5) };
        const Hit h = hit(pad_hz(pad), p, 520);   /* 0.75 s */
        combos++;
        CHECK(h.over == 0, "above the declared peak (%d %d %d %d %d %d) pad %d", a, b, c, d, e, g, pad);
        CHECK(h.peak > 2400, "weak hit peak %d (%d %d %d %d %d %d) pad %d", h.peak, a, b, c, d, e, g, pad);
        if (a == 0) CHECK(h.tail < 30, "short decay still ringing: tail %.0f pad %d", h.tail, pad);
    }
    const opxy::i32 def[8] = { q(0.27), q(0.43), q(0.49), q(0.24), q(0.12), 0, q(0.49), q(0.5) };
    const Hit kick = hit(pad_hz(0), def, 1040), hat = hit(pad_hz(7), def, 1040);
    CHECK(kick.len > hat.len * 1.6, "kick %ld should outlast hat %ld", kick.len, hat.len);
    CHECK(hat.bright > kick.bright * 2, "hat %.3f should be brighter than kick %.3f", hat.bright, kick.bright);
    opxy::i32 pn[8] = { q(0.6), q(1.0), q(1.0), 0, 0, 0, q(0.49), q(0.5) };
    const Hit pk = hit(pad_hz(0), pn, 200);
    CHECK(pk.zc1 > pk.zc2 * 2.5, "pitch drop: early %.0f late %.0f crossings / 10 ms", pk.zc1, pk.zc2);
    pn[1] = 0; const Hit flat = hit(pad_hz(0), pn, 200);
    CHECK(flat.zc1 <= flat.zc2 + 2, "Punch 0 should be flat: early %.0f late %.0f", flat.zc1, flat.zc2);
    opxy::i32 t[8] = { q(0.49), q(0.24), q(0.09), 0, 0, 0, q(0.49), q(0.5) };
    const Hit dark = hit(pad_hz(3), t, 240); t[2] = q(1.0); const Hit open = hit(pad_hz(3), t, 240);
    CHECK(open.bright > dark.bright * 1.5, "Cutoff: dark %.3f open %.3f", dark.bright, open.bright);
    t[3] = q(1.0); const Hit fmh = hit(pad_hz(3), t, 240);
    CHECK(fmh.bright > open.bright * 1.2, "FM: off %.3f on %.3f", open.bright, fmh.bright);
    t[3] = 0; t[4] = q(1.0); const Hit noisy = hit(pad_hz(3), t, 240);
    CHECK(noisy.bright > open.bright * 1.3, "Noise: off %.3f on %.3f", open.bright, noisy.bright);
    /* the hit LFO: Rate "2" flips every hit, Rate "32" takes 32 hits per cycle */
    {
        opxy::i32 lf[8] = { q(0.12), 0, q(0.49), 0, 0, q(1.0), q(1.0), q(0.5) };
        static theseus::Voice v; opxy::u32 pitch[64]; opxy::i32 out[64];
        for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(110.0 / 44100.0 * 4294967296.0);
        int flips = 0, prev = 0;
        for (int k = 0; k < 16; k++) { theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, lf, pitch);
            const int s = v.lfo > 0 ? 1 : -1; if (k && s != prev) flips++; prev = s; }
        CHECK(flips >= 14, "Rate 2: %d sign flips in 16 hits", flips);
        lf[6] = 0; flips = 0;
        for (int k = 0; k < 32; k++) { theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, lf, pitch);
            const int s = v.lfo > 0 ? 1 : -1; if (k && s != prev) flips++; prev = s; }
        CHECK(flips >= 1 && flips <= 3, "Rate 32: %d sign flips in 32 hits", flips);
    }
    /* zones, and Tune moving them */
    {
        static theseus::Voice v; opxy::u32 pitch[64]; opxy::i32 out[64];
        opxy::i32 pz[8] = { q(0.27), q(0.43), q(0.49), q(0.24), 0, 0, q(0.49), q(0.5) };
        for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(55.0 / 44100.0 * 4294967296.0);
        theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, pz, pitch); const int z0 = v.zone;
        for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(880.0 / 44100.0 * 4294967296.0);
        theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, pz, pitch); const int z1 = v.zone;
        pz[7] = q(1.0); theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, pz, pitch); const int z2 = v.zone;
        for (int n = 0; n < 64; n++) pitch[n] = static_cast<opxy::u32>(55.0 / 44100.0 * 4294967296.0);
        pz[7] = q(1.0); theseus::initialize(v, shared, 1); theseus::start(v, shared, 1); theseus::render(v, shared, 1, out, pz, pitch); const int z3 = v.zone;
        CHECK(z0 < 600 && z1 > 31000, "zone 55 Hz %d, 880 Hz %d", z0, z1);
        CHECK(z2 >= z1 && z3 > 10000 && z3 < 24000, "Tune +24 should move 55 Hz up two octaves of zone: %d (880 Hz: %d)", z3, z2);
    }
    std::printf("%ld control/pad combinations; kick %ld / hat %ld samples, bright %.3f / %.3f; pitch drop %.0f->%.0f\n",
                combos, kick.len, hat.len, kick.bright, hat.bright, pk.zc1, pk.zc2);
    std::printf(fails ? "%d FAILED\n" : "all theseus checks passed\n", fails);
    return fails != 0;
}
