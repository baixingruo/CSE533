//
//  CirQueue.h
//  HW2
//
//  Created by Panchen Xue on 10/25/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#ifndef _Cir_Queue_H
#define _Cir_Queue_H

#include <sys/socket.h>
#include "unpthread.h"

struct data {

    uint32_t seq;
    uint32_t ts;
    char sendline[MAXLINE];
    int ack;
};

struct CirQueue {

    struct data *base;
    int front;
    int rear;
    int MAX_QSIZE;
};

void InitCirQueue(struct CirQueue *Q, int size);

void DestroyCirQueue(struct CirQueue *Q);

void ClearCirQueue(struct CirQueue *Q);

int IsEmptyCirQueue(struct CirQueue *Q);

int CirQueueLength(struct CirQueue *Q);

void EnCirQueue(struct CirQueue *Q, struct data input);

void DeCirQueue(struct CirQueue *Q);

#endif

