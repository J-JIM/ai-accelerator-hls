#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "gemm.h"

using namespace std;

int main(void){

    float *a = (float *)malloc(sizeof(float)*(M_SIZE * N_SIZE));
    float *b = (float *)malloc(sizeof(float)*(N_SIZE * O_SIZE));
    float *c = (float *)malloc(sizeof(float)*(M_SIZE * O_SIZE));
    float *golden = (float *)malloc(sizeof(float) * M_SIZE * O_SIZE);
    //random data
    for(int i =0 ; i < M_SIZE * N_SIZE; i++) a[i] = (float)(rand() % 1024) - 512;
    for(int i =0 ; i < N_SIZE * O_SIZE; i++) b[i] = (float)(rand() % 1024) - 512;
    for(int i =0 ; i < M_SIZE * O_SIZE; i++) {c[i] = 0; golden[i] = 0;}

    gemm(a,b,c);
    
    for(int m = 0; m < M_SIZE; m++){
        for(int n = 0; n < N_SIZE; n++){
            for(int o = 0; o < O_SIZE; o++){
                golden[m*O_SIZE + o] += a[m*N_SIZE + n] * b[n*O_SIZE + o];
            }
        }
    }

    bool flag = false;
    for(int i = 0; i < M_SIZE * O_SIZE; i++){
        if(c[i] != golden[i]){flag = true; break;}
    }
    if(flag){
        cout << "fail test bench";
        return -1;
    }else{
        cout << "success test bench";
        return 0;
    }
}