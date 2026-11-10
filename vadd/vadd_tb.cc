#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "vadd.h"

int main(void){
    bool correct = true;

    float *a = (float *) malloc(sizeof(float) * VECTOR_LEN);
    float *b = (float *) malloc(sizeof(float) * VECTOR_LEN);
    float *c = (float *) malloc(sizeof(float) * VECTOR_LEN);

    for(int i = 0; i < VECTOR_LEN; i++){
        a[i] = (float)(rand()%1024) - 512 ;
        b[i] = (float)(rand()%1024) - 512;
        c[i] = (float)(rand()%1024) - 512;
    }
    vadd(a,b,c);

    for(int i =0 ; i <VECTOR_LEN; i++){
        if(c[i] != a[i] + b[i]) {correct = false; break;}
    }

    free(a);
    free(b);
    free(c);

    if(correct){
        printf("Test successful\n");
        return 0;
    } else{
        printf("Test unsuccessful\n");
        return -1;
    }

}