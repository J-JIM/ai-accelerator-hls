#ifndef GEMM_TILED_H_
#define GEMM_TILED_H_



// tile sizes (buffer sizes).
#define T_M 5
#define T_N 32
#define T_O 32


#define MAX_ELEM 4096

void gemm_tiled(
        float *a,
        float *b,
        float *c,
        int M,
        int N,
        int O);

#endif
