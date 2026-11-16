#include "gemm_tiled.h"

// C = A x B with tiling.
//   for each output tile (mt, ot):
//       c_buff = 0
//       for each input tile pair along N (nt):
//           a_buff <- A[mt.., nt..]   (cells outside the matrix are filled with 0)
//           b_buff <- B[nt.., ot..]   (same)
//           c_buff += a_buff x b_buff (full T_M x T_N x T_O, hardware is reused as is)
//       C[mt.., ot..] <- c_buff       (only cells inside the matrix are written)
//
// Zero padding keeps one fixed matmul circuit for every tile, including the
// last partial tile when M, N or O is not a multiple of the tile size:
// 0 * x = 0 is accumulated, so the result does not change.

void gemm_tiled(
        float *a,
        float *b,
        float *c,
        int M,
        int N,
        int O){
#pragma HLS INTERFACE m_axi port = a depth = MAX_ELEM offset = slave bundle = a_port
#pragma HLS INTERFACE m_axi port = b depth = MAX_ELEM offset = slave bundle = b_port
#pragma HLS INTERFACE m_axi port = c depth = MAX_ELEM offset = slave bundle = c_port

#pragma HLS INTERFACE s_axilite port = a bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = b bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = c bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = M bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = N bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = O bundle = CONTROL_BUS
#pragma HLS INTERFACE s_axilite port = return bundle = CONTROL_BUS

    float a_buff[T_M][T_N];
    float b_buff[T_N][T_O];
    float c_buff[T_M][T_O];

    // Optimization (default build): one MAC per oo column -> each column needs
    // its own memory bank. Build with -DBASELINE to drop UNROLL/PARTITION and
    // get the tiled baseline for the report comparison.
#ifndef BASELINE
#pragma HLS ARRAY_PARTITION variable = b_buff dim = 2 type = complete
#pragma HLS ARRAY_PARTITION variable = c_buff dim = 2 type = complete
#endif

    // number of tiles in each direction (ceil)
    const int TILES_M = (M + T_M - 1) / T_M;
    const int TILES_N = (N + T_N - 1) / T_N;
    const int TILES_O = (O + T_O - 1) / T_O;

tile_m: for (int mt = 0; mt < TILES_M; mt++){
#pragma HLS LOOP_TRIPCOUNT min = 1 max = 8
    tile_o: for (int ot = 0; ot < TILES_O; ot++){
#pragma HLS LOOP_TRIPCOUNT min = 1 max = 8
        const int m0 = mt * T_M;
        const int o0 = ot * T_O;

    init_output: for (int mm = 0; mm < T_M; mm++){
            for (int oo = 0; oo < T_O; oo++){
#pragma HLS PIPELINE II = 1
                c_buff[mm][oo] = 0;
            }
        }

    tile_n: for (int nt = 0; nt < TILES_N; nt++){
#pragma HLS LOOP_TRIPCOUNT min = 1 max = 8
            const int n0 = nt * T_N;

        load_input: for (int mm = 0; mm < T_M; mm++){
                for (int nn = 0; nn < T_N; nn++){
#pragma HLS PIPELINE II = 1
                    const int m = m0 + mm;
                    const int n = n0 + nn;
                    const bool in = (m < M) && (n < N);
                    float v = 0;
                    if (in) v = a[m * N + n];
                    a_buff[mm][nn] = v;
                }
            }

        load_weight: for (int nn = 0; nn < T_N; nn++){
                for (int oo = 0; oo < T_O; oo++){
#pragma HLS PIPELINE II = 1
                    const int n = n0 + nn;
                    const int o = o0 + oo;
                    const bool in = (n < N) && (o < O);
                    float v = 0;
                    if (in) v = b[n * O + o];
                    b_buff[nn][oo] = v;
                }
            }

        matmul: for (int mm = 0; mm < T_M; mm++){
                for (int nn = 0; nn < T_N; nn++){
#ifndef BASELINE
#pragma HLS PIPELINE II = 1
#endif
                    const float av = a_buff[mm][nn];
                matmul_o: for (int oo = 0; oo < T_O; oo++){
#ifndef BASELINE
#pragma HLS UNROLL
#else
#pragma HLS PIPELINE II = 1
#endif
                        c_buff[mm][oo] += av * b_buff[nn][oo];
                    }
                }
            }
        }

    store_output: for (int mm = 0; mm < T_M; mm++){
            for (int oo = 0; oo < T_O; oo++){
#pragma HLS PIPELINE II = 1
                const int m = m0 + mm;
                const int o = o0 + oo;
                if (m < M && o < O) c[m * O + o] = c_buff[mm][oo];
            }
        }
    }
    }
}
