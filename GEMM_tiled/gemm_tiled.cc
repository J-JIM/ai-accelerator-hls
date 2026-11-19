#include "gemm_tiled.h"

void load_a(float *a, float a_t[T_M][T_N], int m0, int n0, int M, int N, bool valid){
    bool full = valid && (M - m0 >= T_M) && (N - n0 >= T_N);

    if (full){
    load_a_full: for (int mm = 0; mm < T_M; mm++){
            for (int nn = 0; nn < T_N; nn++){
                a_t[mm][nn] = a[(m0 + mm) * N + n0 + nn];
            }
        }
    }
    else if (valid){
    load_a_edge: for (int mm = 0; mm < T_M; mm++){
            for (int nn = 0; nn < T_N; nn++){
                int m = m0 + mm;
                int n = n0 + nn;
                float v = 0;
                if (m < M && n < N) v = a[m * N + n];
                a_t[mm][nn] = v;
            }
        }
    }
}

void load_b(float *b, float b_t[T_N][T_O], int n0, int o0, int N, int O, bool valid){
    bool full = valid && (N - n0 >= T_N) && (O - o0 >= T_O);

    if (full){
    load_b_full: for (int nn = 0; nn < T_N; nn++){
            for (int oo = 0; oo < T_O; oo++){
                b_t[nn][oo] = b[(n0 + nn) * O + o0 + oo];
            }
        }
    }
    else if (valid){
    load_b_edge: for (int nn = 0; nn < T_N; nn++){
            for (int oo = 0; oo < T_O; oo++){
                int n = n0 + nn;
                int o = o0 + oo;
                float v = 0;
                if (n < N && o < O) v = b[n * O + o];
                b_t[nn][oo] = v;
            }
        }
    }
}

void compute_tile(float a_t[T_M][T_N], float b_t[T_N][T_O], float c_t[T_M][T_O], bool first){
matmul: for (int nn = 0; nn < T_N; nn++){
        for (int mm = 0; mm < T_M; mm++){
        matmul_o: for (int oo = 0; oo < T_O; oo++){
                #pragma HLS PIPELINE
                #pragma HLS UNROLL factor=16
                float acc = c_t[mm][oo];
                if (first && nn == 0) acc = 0;
                c_t[mm][oo] = acc + a_t[mm][nn] * b_t[nn][oo];
            }
        }
    }
}

void store_tile(float *c, float c_t[T_M][T_O], int m0, int o0, int M, int O){
store_output: for (int mm = 0; mm < T_M; mm++){
        for (int oo = 0; oo < T_O; oo++){
            int m = m0 + mm;
            int o = o0 + oo;
            if (m < M && o < O) c[m * O + o] = c_t[mm][oo];
        }
    }
}

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

    float a0[T_M][T_N];
    float a1[T_M][T_N];
    float b0[T_N][T_O];
    float b1[T_N][T_O];
    float c_buff[T_M][T_O];
    #pragma HLS ARRAY_PARTITION variable=b0 dim=2 type=cyclic factor=16
    #pragma HLS ARRAY_PARTITION variable=b1 dim=2 type=cyclic factor=16
    #pragma HLS ARRAY_PARTITION variable=c_buff dim=2 type=cyclic factor=16

    int TILES_M = (M + T_M - 1) / T_M;
    int TILES_N = (N + T_N - 1) / T_N;
    int TILES_O = (O + T_O - 1) / T_O;

    int total = TILES_M * TILES_O * TILES_N;
    if (M <= 0 || N <= 0 || O <= 0) total = 0;

    load_a(a, a0, 0, 0, M, N, total > 0);
    load_b(b, b0, 0, 0, N, O, total > 0);

    int mt = 0;
    int ot = 0;
    int nt = 0;

step: for (int s = 0; s < total; s++){
        int next_mt = mt;
        int next_ot = ot;
        int next_nt = nt + 1;
        if (next_nt == TILES_N){
            next_nt = 0;
            next_ot = ot + 1;
            if (next_ot == TILES_O){
                next_ot = 0;
                next_mt = mt + 1;
            }
        }

        bool has_next = (s + 1 < total);
        bool first = (nt == 0);
        bool last = (nt == TILES_N - 1);

        if (s % 2 == 0){
            load_a(a, a1, next_mt * T_M, next_nt * T_N, M, N, has_next);
            load_b(b, b1, next_nt * T_N, next_ot * T_O, N, O, has_next);
            compute_tile(a0, b0, c_buff, first);
        }
        else {
            load_a(a, a0, next_mt * T_M, next_nt * T_N, M, N, has_next);
            load_b(b, b0, next_nt * T_N, next_ot * T_O, N, O, has_next);
            compute_tile(a1, b1, c_buff, first);
        }
        
        if (last) store_tile(c, c_buff, mt * T_M, ot * T_O, M, O);

        mt = next_mt;
        ot = next_ot;
        nt = next_nt;
    }
}
