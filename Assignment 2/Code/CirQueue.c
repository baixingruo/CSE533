//
//  CirQueue.c
//  HW2
//
//  Created by Panchen Xue on 10/25/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>  
#include <malloc.h>
#include "CirQueue.h"
 
void InitCirQueue(struct CirQueue *Q, int size) {

    Q->MAX_QSIZE=size;
    Q->base=(struct data *) malloc(Q->MAX_QSIZE*sizeof(struct data));

    if(!Q->base) exit(1);

    Q->front=Q->rear=0;
}
 
void DestroyCirQueue(struct CirQueue *Q) {

    if(Q->base) free(Q->base);
    Q->base=NULL;
    Q->front=Q->rear=0;
}
 
void ClearCirQueue(struct CirQueue *Q) {
 
    Q->front=Q->rear=0;
}

int IsEmptyCirQueue(struct CirQueue *Q) {

    if(Q->front==Q->rear) return 1;
    else return 0;
}
 
int CirQueueLength(struct CirQueue *Q) {

    return (Q->rear-Q->front+Q->MAX_QSIZE)%Q->MAX_QSIZE;
}
 
void EnCirQueue(struct CirQueue *Q, struct data input) {

    Q->base[Q->rear]=input;
    Q->rear=(Q->rear+1)%Q->MAX_QSIZE;
}
 
void DeCirQueue(struct CirQueue *Q) {

    Q->front=(Q->front+1)%Q->MAX_QSIZE;
}

