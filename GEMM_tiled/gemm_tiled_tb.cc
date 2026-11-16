#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "gemm_tiled.h"

using namespace std;

// Runs one case: fills A, B with random values in [-512, 511], poisons C,
// calls the IP, compares every C cell with a golden C model that accumulates
// over n in the same ascending order as the hardware.
static int run_case(float *a, float *b, float *c, float *golden,
                    int M, int N, int O, const char *label){
    // whole buffers get random data: cells beyond M*N / N*O are garbage the IP
    // must never use (checks the zero padding of the last partial tile)
    for (int i = 0; i < MAX_ELEM; i++) a[i] = (float)(rand() % 1024) - 512;
    for (int i = 0; i < MAX_ELEM; i++) b[i] = (float)(rand() % 1024) - 512;
    // poison: every C cell must be overwritten by the IP
    for (int i = 0; i < MAX_ELEM; i++) c[i] = 12345.0f;
    for (int i = 0; i < M * O; i++) golden[i] = 0;

    gemm_tiled(a, b, c, M, N, O);

    for (int m = 0; m < M; m++)
        for (int n = 0; n < N; n++)
            for (int o = 0; o < O; o++)
                golden[m * O + o] += a[m * N + n] * b[n * O + o];

    int bad = 0;
    for (int m = 0; m < M; m++){
        for (int o = 0; o < O; o++){
            if (c[m * O + o] != golden[m * O + o]){
                if (bad < 5)
                    printf("  mismatch at c[%d][%d]: got %f expected %f\n",
                           m, o, c[m * O + o], golden[m * O + o]);
                bad++;
            }
        }
    }
    // cells after M*O must keep the poison value (IP must not write outside C)
    int overrun = 0;
    for (int i = M * O; i < MAX_ELEM; i++) if (c[i] != 12345.0f) overrun++;

    printf("[%s] M=%d N=%d O=%d (tiles %dx%dx%d) : %s  (mismatch %d, overrun %d)\n",
           label, M, N, O,
           (M + T_M - 1) / T_M, (N + T_N - 1) / T_N, (O + T_O - 1) / T_O,
           (bad == 0 && overrun == 0) ? "PASS" : "FAIL", bad, overrun);
    return (bad == 0 && overrun == 0) ? 0 : 1;
}

int main(void){
    // allocate the full depth so co-simulation can read/write depth elements
    float *a      = (float *)malloc(sizeof(float) * MAX_ELEM);
    float *b      = (float *)malloc(sizeof(float) * MAX_ELEM);
    float *c      = (float *)malloc(sizeof(float) * MAX_ELEM);
    float *golden = (float *)malloc(sizeof(float) * MAX_ELEM);
    int fails = 0;
    // 1) same size as p2 GEMM (M = 10 is already not a multiple of 8)
    fails += run_case(a, b, c, golden, 10, 64, 64, "p2 size   ");
    // 2) every dimension a multiple of the tile size
    fails += run_case(a, b, c, golden, 16, 32, 24, "divisible ");
    // 3) none of M, N, O divisible by 8 (professor's 26 example included)
    fails += run_case(a, b, c, golden, 13, 26, 18, "remainder ");
    // 4) smaller than one tile in every dimension
    fails += run_case(a, b, c, golden,  3,  5,  7, "tiny      ");

    if (fails){
        cout << "fail test bench (" << fails << " case(s))" << endl;
        return -1;
    }
    cout << "success test bench" << endl;
    return 0;
}
