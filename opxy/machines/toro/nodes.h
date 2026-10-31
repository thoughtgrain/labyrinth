#pragma once
// toro for the OP-XY: node indices, written by gen_panel.py. Edit the generator, not this file.
namespace toro {
enum {
    N_PATH0 = 0, N_PATH0_COUNT = 1,
    N_PATH1 = 1, N_PATH1_COUNT = 1,
    N_HDR = 2, N_HDR_COUNT = 8,
    N_ALD = 10, N_ALD_COUNT = 3,
    N_S_G1 = 13, N_S_G1_COUNT = 1,
    N_S_G2 = 14, N_S_G2_COUNT = 1,
    N_S_G3 = 15, N_S_G3_COUNT = 1,
    N_S_G4 = 16, N_S_G4_COUNT = 1,
    N_S_BETA = 17, N_S_BETA_COUNT = 1,
    N_S_ZETA = 18, N_S_ZETA_COUNT = 1,
    N_S_BACK = 19, N_S_BACK_COUNT = 1,
    N_S_TAIL = 20, N_S_TAIL_COUNT = 1,
    N_JOIN = 21, N_JOIN_COUNT = 9,
    N_PLE = 30, N_PLE_COUNT = 7,
    N_PLE_RING = 37, N_PLE_RING_COUNT = 10,
    N_COMET0 = 47, N_COMET0_COUNT = 2,
    N_COMET1 = 49, N_COMET1_COUNT = 2,
    N_COMET2 = 51, N_COMET2_COUNT = 2,
    N_COMET3 = 53, N_COMET3_COUNT = 2,
    N_TWINKLE = 55, N_TWINKLE_COUNT = 12,
    N_BAND = 67, N_BAND_COUNT = 1,
    N_TOTAL = 68
};
}

// chart geometry (pixels relative to the chart centre; twinkle positions absolute)
namespace toro {
const int CHART_CX = 240, CHART_CY = 134;
enum { ST_ALD, ST_G1, ST_G2, ST_G3, ST_G4, ST_BETA, ST_ZETA, ST_BACK, ST_TAIL, ST_N };
const int STAR_X[ST_N] = { 18, -6, -26, -4, -28, 110, 96, -70, -150 };
const int STAR_Y[ST_N] = { 2, -14, -24, 16, 28, -56, 44, -6, 22 };
const int STAR_NODE[ST_N] = { N_ALD + 1, N_S_G1, N_S_G2, N_S_G3, N_S_G4, N_S_BETA, N_S_ZETA, N_S_BACK, N_S_TAIL };
const int JOIN_N = 9;
const int JOIN_A[JOIN_N] = { ST_ALD, ST_G1, ST_ALD, ST_G3, ST_G1, ST_G3, ST_G2, ST_G4, ST_BACK };
const int JOIN_B[JOIN_N] = { ST_G1, ST_G2, ST_G3, ST_G4, ST_BETA, ST_ZETA, ST_BACK, ST_BACK, ST_TAIL };
const int PLE_X = -120, PLE_Y = -28;
const int PLE_DX[7] = { -8, -2, 5, 9, 3, -5, 0 };
const int PLE_DY[7] = { -4, -7, -5, 1, 5, 6, 0 };
const int TW_X[12] = { 440, 32, 192, 337, 23, 150, 142, 466, 378, 444, 359, 452 };
const int TW_Y[12] = { 125, 78, 189, 47, 70, 84, 75, 157, 201, 164, 104, 93 };
}
