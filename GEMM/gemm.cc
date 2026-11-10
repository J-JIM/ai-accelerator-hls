#include "./gemm.h"


void gemm(
        float *a,
        float *b,
        float *c){
            // make interface 
            #pragma HLS INTERFACE m_axi port = a depth = 640 offset = slave bundle = a_port
            #pragma HLS INTERFACE m_axi port = b depth = 4096 offset = slave bundle = b_port
            #pragma HLS INTERFACE m_axi port = c depth = 640 offset = slave bundle = c_port

            #pragma HLS INTERFACE s_axilite port = a bundle = CONTROL_BUS
            #pragma HLS INTERFACE s_axilite port = b bundle = CONTROL_BUS
            #pragma HLS INTERFACE s_axilite port = c bundle = CONTROL_BUS

            #pragma HLS INTERFACE s_axilite port = return bundle = CONTROL_BUS

            // M by N matrix and N by O matrix multiplication output c_buff = M by O matrix
            float buffer_a[M_SIZE][N_SIZE];
            float buffer_b[N_SIZE][O_SIZE];
            float buffer_c[M_SIZE][O_SIZE];

            #pragma HLS ARRAY_PARTITION variable=buffer_b dim=2 type=cyclic factor=8
            #pragma HLS ARRAY_PARTITION variable=buffer_c dim=2 type=cyclic factor=8
           
            // load data
           rdata_a: for(int m = 0; m < M_SIZE; m++){
            cdata_a:for(int n = 0; n < N_SIZE; n++){
                
                buffer_a[m][n] = a[m*N_SIZE + n];
            }
           }

           rdata_b: for(int n= 0; n < N_SIZE; n++){
            cdata_b: for(int o = 0; o < O_SIZE; o++){
                
                buffer_b[n][o] = b[n*O_SIZE + o];
            }
           }

           rdata_c: for(int m =0; m < M_SIZE; m++){
            cdata_c : for(int o = 0; o < O_SIZE; o++){
                buffer_c[m][o] = 0;
            }
           }

            // matrix multiple buffer_c
            wrdata_c: for(int m = 0; m <M_SIZE; m++){
                wrcdata_c: for(int n = 0; n < N_SIZE; n++){
                        idx_n: for (int o = 0; o < O_SIZE; o++){
                            #pragma HLS PIPELINE 
                            #pragma HLS UNROLL factor=8
                            buffer_c[m][o] += buffer_a[m][n] * buffer_b[n][o];
                    }
                }
            }
            
            //store memory c
            write_rc: for (int m = 0; m < M_SIZE; m++){
                write_cc : for(int o = 0; o <O_SIZE; o++){
                    c[m*O_SIZE+o] = buffer_c[m][o];
                }
            }
            return;
}


