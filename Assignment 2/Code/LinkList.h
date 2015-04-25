//
//  LinkList.h
//  HW2
//
//  Created by Panchen Xue on 11/1/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#ifndef _Link_List_H
#define _Link_List_H

#include <sys/socket.h>
#include "unpthread.h"

struct node {

	uint32_t seq;
	char recvline[MAXLINE+3];
	struct node *next;
};

void InsertLinkList(struct node *L, uint32_t seq, char data[]);

void DeleteLinkList(struct node *L);

void DeleteSelectedNode(struct node *L, uint32_t port, char ip_addr[]);

int DuplicateNode(struct node *L, uint32_t port, char ip_addr[]);

int LinkListLength(struct node *L);

int NextAck(struct node *L, int start);

#endif

