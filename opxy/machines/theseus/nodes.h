#pragma once
// theseus for the OP-XY: node indices and the tower's geometry, written by gen_panel.py.
// Edit the generator, not this file.
namespace theseus {
enum { N_SLICE = 0, N_HDR = 1, N_DASH = 9, N_OVER = 130, OVER_R = 62, N_TIERS = 134, N_TOTAL = 232 };
enum { P_BLACK = 0, P_SHELL = 1, P_SHELL_HOT = 2, P_HEAD = 3, P_LIT1 = 4, P_LIT2 = 5, P_LIT3 = 6, P_LIT4 = 7, P_LIT5 = 8, P_DIM = 9, P_G4 = 10, P_G5 = 11, P_G6 = 12, P_HOT = 13, P_MOUNT = 14, P_SLICE = 15, PALETTE_N = 16 };
const int TOWER_X = 240, FLOOR_Y = 195;     // the floor clock's centre, px
const int FLOOR_RX = 94208, FLOOR_RY = 24003;   // the slice's radii, Q10 px
const int RATE_N = 10;
const unsigned char RATE_HITS[RATE_N] = { 32, 24, 18, 14, 10, 8, 6, 4, 3, 2 };
const short DASH_FIRST[RATE_N] = { 9, 41, 65, 83, 97, 107, 115, 121, 125, 128 };
// the drums, bottom to top: their nodes, rest heads, radii and the zone each one plays
enum { DRUM_N = 4, TIER_N = 5, HAT = 4 };
const short DRUM_FIRST[DRUM_N] = { 134, 152, 165, 184 };
const short DRUM_COUNT[DRUM_N] = { 18, 13, 19, 11 };
const short DRUM_BODY[DRUM_N] = { 134, 154, 169, 184 };
const short DRUM_BODY_REST[DRUM_N] = { 1, 1, 3, 1 };
const short DRUM_BATTER[DRUM_N] = { 146, 160, 179, 190 };
const short DRUM_RING[DRUM_N] = { 147, 161, 180, 191 };
const short DRUM_TOP[DRUM_N] = { 164, 133, 114, 90 };
const short DRUM_R[DRUM_N] = { 86, 62, 50, 38 };              // px
const short DRUM_RY[DRUM_N] = { 22897, 16507, 13312, 10117 };       // Q10 px
const short TIER_ZONE[TIER_N] = { 20, 256, 461, 666, 901 };   // Q10
const int HAT_BASE = 75, HAT_R = 36;
enum { HAT_ROD = 195, HAT_LOW = 196, HAT_TOP = 200, HAT_PARTS = 4, HAT_SHIM = 204, SPARK = 208, SPARK_N = 24 };
// every node's origin y and rest shade, from N_TIERS up to the sparks (every origin sits on x = TOWER_X)
const short NODE_OY[SPARK - N_TIERS] = { 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 75, 77, 77, 77, 77, 72, 72, 72, 72, 72, 72, 72, 72 };
const unsigned char NODE_SHADE[SPARK - N_TIERS] = { 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 3, 13, 12, 12, 12, 9, 9, 9, 1, 9, 9, 9, 9, 9, 3, 13, 12, 12, 12, 9, 9, 9, 9, 3, 9, 12, 12, 12, 12, 12, 12, 12, 12, 3, 13, 12, 12, 12, 1, 9, 14, 9, 9, 9, 3, 13, 12, 12, 12, 9, 3, 9, 9, 12, 3, 9, 9, 12, 12, 12, 12, 12 };
}
