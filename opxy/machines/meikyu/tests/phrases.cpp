/* meikyu player: host checks of the phrase logic (sdk.py test compiles and runs this). */
#include "../player.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define require(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
using namespace opxy;
using namespace opxy::player;
namespace {
const i32 top = 0x7fffffff, half = 0x40000000;
struct Rig {
    meikyu::State state = {};
    Controls controls = { { half + half / 5, 0, half / 2, 0 }, 53 };   /* Dens 60, minor, Crpt 25, drum off */
    Plan plan = {};
    void dens(u32 p) { controls.value[0] = static_cast<i32>((static_cast<long long>(p) * top) / 100); }
    void crpt(u32 p) { controls.value[2] = static_cast<i32>((static_cast<long long>(p) * top) / 100); }
    void scale(u32 k) { controls.value[1] = static_cast<i32>((static_cast<long long>(k) * 2 + 1) * (top / 8)); }
    void drum(bool on) { controls.value[3] = on ? top : 0; }
    void on(u32 note, u32 velocity = 90) { require(meikyu::press(state, controls, note, velocity, plan)); }
    bool off(u32 note) { return meikyu::release(state, note); }
};
bool in_scale(i32 pitch, i32 root, u16 mask) { return (mask >> (((pitch - root) % 12 + 12) % 12)) & 1; }

void deterministic_first_phrase()
{
    Rig a, b;
    a.on(60); b.on(60);
    require(a.plan.count == b.plan.count && a.plan.count > 0);
    for (u32 i = 0; i < a.plan.count; ++i) require(a.plan.tones[i].pitch == b.plan.tones[i].pitch && a.plan.tones[i].delay == b.plan.tones[i].delay);
}
void density_bounds()
{
    Rig r; r.dens(0); r.crpt(0);
    r.on(60); require(r.plan.count == 0);
    require(r.off(60));                                  /* a silent phrase: nothing for stock to release */
    r.dens(100);
    r.on(60); require(r.plan.count == 8);
    for (u32 k = 0; k < 8; ++k) { require(r.plan.tones[k].delay == k); require(r.plan.tones[k].pitch >= 60); require(in_scale(r.plan.tones[k].pitch, 60, 0x5AD)); }
    require(!r.off(60));                                 /* a sounding phrase: stock releases its chord */
}
void scales_and_drums()
{
    const u16 masks[4] = { 0x5AD, 0xAB5, 0x4A9, 0xFFF };
    for (u32 k = 0; k < 4; ++k) {
        Rig r; r.dens(100); r.crpt(0); r.scale(k);
        r.on(48, 127);
        require(r.plan.count == 8);
        for (u32 i = 0; i < 8; ++i) require(in_scale(r.plan.tones[i].pitch, 48, masks[k]) && r.plan.tones[i].pitch <= 48 + 36);
    }
    Rig r; r.dens(100); r.crpt(0); r.drum(true);
    r.on(36, 127);
    const u8 pads[8] = { 0, 5, 7, 12, 17, 24, 31, 36 };
    for (u32 i = 0; i < 8; ++i) { bool pad = false; for (u8 p : pads) if (r.plan.tones[i].pitch == 36 + p) pad = true; require(pad); }
}
void velocity_is_range()
{
    Rig soft, hard; soft.dens(100); soft.crpt(0); hard.dens(100); hard.crpt(0);
    soft.on(60, 1); hard.on(60, 127);
    u32 soft_max = 0, hard_max = 0;
    for (u32 i = 0; i < 8; ++i) { if (soft.plan.tones[i].pitch > soft_max) soft_max = soft.plan.tones[i].pitch; if (hard.plan.tones[i].pitch > hard_max) hard_max = hard.plan.tones[i].pitch; }
    require(soft_max <= 60 + 7);                         /* 3 degrees at most: within a fifth */
    require(hard_max > soft_max);
    for (u32 i = 0; i < 8; ++i) require(soft.plan.tones[i].velocity == 1 || soft.plan.tones[i].velocity == 28);   /* accents lift the velocity */
}
void frozen_pattern_repeats_every_two_phrases()
{
    Rig r; r.dens(100); r.crpt(0);
    r.on(60); Plan first = r.plan; r.off(60);
    r.on(60); Plan second = r.plan; r.off(60);
    r.on(60); Plan third = r.plan; r.off(60);
    bool same = true;
    for (u32 i = 0; i < 8; ++i) same = same && first.tones[i].pitch == third.tones[i].pitch;
    require(same);
    bool differs = false;
    for (u32 i = 0; i < 8; ++i) differs = differs || first.tones[i].pitch != second.tones[i].pitch;
    require(differs);                                    /* the two halves of the register are different steps */
    require(r.state.rewrote == 0);
}
void corruption_drifts()
{
    Rig r; r.dens(100); r.crpt(100);
    r.on(60); r.off(60); r.on(60); r.off(60);
    require(r.state.rewrote == 0xFFFF);                  /* every step passed was rolled again */
    Rig frozen; frozen.dens(100); frozen.crpt(0);
    frozen.on(60); frozen.off(60);
    u8 before[16]; std::memcpy(before, frozen.state.val, 16);
    frozen.on(60); frozen.off(60); frozen.on(60);
    require(std::memcmp(before, frozen.state.val, 16) == 0);   /* Crpt 0 freezes the register */
}
void eight_roots()
{
    Rig r; r.dens(100); r.crpt(0);
    for (u32 n = 0; n < 8; ++n) { r.on(48 + n); require(r.plan.count == 8); }
    r.on(70); require(r.plan.count == 0);                /* a ninth is dropped */
    require(!r.off(50));
    r.on(70); require(r.plan.count == 8);                /* the freed slot is reused */
    require(!r.off(70));
    require(!r.off(99));                                 /* an unknown note: let stock decide */
    meikyu::reset(r.state);
    for (u32 i = 0; i < 8; ++i) require(r.state.roots[i].note == 0);
    require(r.state.initialized);                        /* the register survives a stop */
}
void page()
{
    Rig r; r.dens(100); r.crpt(0);
    Display d = {};
    meikyu::display(r.state, r.controls, d);
    require(std::strcmp(d.text, "meikyu") == 0 && d.lit == 0);
    r.on(62);
    meikyu::display(r.state, r.controls, d);
    require((d.lit & 0xFFFF) == 0xFFFF && (d.lit >> 16) == 0xFF);
    require(std::strcmp(d.text, "D A phrase 1") == 0);
    require(std::strcmp(d.values[0], "100") == 0 && std::strcmp(d.values[1], "min") == 0 && std::strcmp(d.values[2], "0") == 0 && std::strcmp(d.values[3], "off") == 0);
    r.on(62);
    meikyu::display(r.state, r.controls, d);
    require(std::strcmp(d.text, "D B phrase 2") == 0);
}
}
int main()
{
    deterministic_first_phrase();
    density_bounds();
    scales_and_drums();
    velocity_is_range();
    frozen_pattern_repeats_every_two_phrases();
    corruption_drifts();
    eight_roots();
    page();
    std::printf("meikyu player: phrases ok\n");
    return 0;
}
