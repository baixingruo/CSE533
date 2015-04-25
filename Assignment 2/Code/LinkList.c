//
//  LinkList.c
//  HW2
//
//  Created by Panchen Xue on 11/1/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "LinkList.h"

void InsertLinkList(struct node *L, uint32_t seq, char recvline[]) {

	if(L->next==NULL) {

		struct node *new_node=(struct node *) malloc(sizeof(struct node));
		new_node->seq=seq;
		strcpy(new_node->recvline, recvline);
		new_node->next=NULL;
		L->next=new_node;
	}
	else {

		struct node *head=L;

		while(head->next!=NULL && head->next->seq<seq) head=head->next;

		if(head->next==NULL) {

			struct node *new_node=(struct node *) malloc(sizeof(struct node));
			new_node->seq=seq;
			strcpy(new_node->recvline, recvline);
			head->next=new_node;
		}
		else if(head->next->seq!=seq) {

			struct node *temp=head->next;

			struct node *new_node=(struct node *) malloc(sizeof(struct node));
			new_node->seq=seq;
			strcpy(new_node->recvline, recvline);
			head->next=new_node;
			new_node->next=temp;			
		}
	}
}

void DeleteLinkList(struct node *L) {

	struct node *temp=L->next;

	L->next=L->next->next;

	free(temp); 
}

void DeleteSelectedNode(struct node *L, uint32_t port, char ip_addr[]) {

	struct node *front=L;
	struct node *rear=L->next;

	while(rear!=NULL && (rear->seq!=port || strcmp(rear->recvline, ip_addr)!=0)) {

		front=rear;
		rear=front->next;
	}

	if(rear!=NULL) {

		front->next=rear->next;
		free(rear);
	}
}

int DuplicateNode(struct node *L, uint32_t port, char ip_addr[]) {

	struct node *front=L;
	struct node *rear=L->next;

	while(rear!=NULL && (rear->seq!=port || strcmp(rear->recvline, ip_addr)!=0)) {

		front=rear;
		rear=front->next;
	}

	if(rear!=NULL) return 1;
	else return 0;
}

int LinkListLength(struct node *L) {

	struct node *temp=L->next; 
    int len=0;  
 
    while(temp!=NULL) {

        temp=temp->next;  
        len++;           
    }

    return len;  
}

int NextAck(struct node *L, int start) {

	struct node *temp=L->next;
	int Ack=start;

	while(temp!=NULL && temp->seq==Ack+1) {

		Ack=temp->seq;
		temp=temp->next;
	}

	return ++Ack;
}

