#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>

#ifndef PART1_GEMM_H_
#define PART1_GEMM_H_
#define VECTOR_LEN 4096
#define M_SIZE 10
#define N_SIZE 64
#define O_SIZE 64

void gemm(
        float *a,
        float *b,
        float *c);

#endif