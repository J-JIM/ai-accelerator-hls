#ifndef GEMM_TILED_H_
#define GEMM_TILED_H_

// Tiled GEMM: C[M][O] = A[M][N] x B[N][O], float, row-major.
// M, N, O are runtime inputs (s_axilite). The on-chip buffers hold one tile only.

// tile sizes (buffer sizes). Resource usage depends on these, not on M/N/O.
#define T_M 8
#define T_N 8
#define T_O 8

// upper bound of elements that can travel through each m_axi port
// (= depth in the INTERFACE pragma; test bench allocates this many elements)
#define MAX_ELEM 4096

void gemm_tiled(
        float *a,
        float *b,
        float *c,
        int M,
        int N,
        int O);

#endif
