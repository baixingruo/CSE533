//
//  server.c
//  HW3
//
//  Created by Panchen Xue on 11/11/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include "hw_addrs.h"
#include "API.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <unistd.h>
#include <time.h>

char* vm_addr[]={
	"130.245.156.21",
	"130.245.156.22",
	"130.245.156.23",
	"130.245.156.24",
	"130.245.156.25",
	"130.245.156.26",
	"130.245.156.27",
	"130.245.156.28",
	"130.245.156.29",
	"130.245.156.20",
	};

int main(int argc, char **argv) {

	printf("\nWELCOME! THIS IS SERVER SERVICE\n\n");

	struct hwa_info *hwa;
    struct sockaddr *sa;
	int node;
	int index;

	if((hwa=get_hw_addrs())==NULL) {

		perror("SERVER: Failed to get interface information for this node\n");
		exit(1);
	}

	sa=hwa->hwa_next->ip_addr;

	//get node index
	for(index=0; index<10; index++) {

		if(strcmp(Sock_ntop_host(sa, sizeof(*sa)), vm_addr[index])==0) {
		
			node=index+1;
			break;
		}
	}
	printf("You are on the VM%d node now\n\n", node);

	int sockfd;
	struct sockaddr_un srvaddr;

	//UNIX domain socket to communicate with local ODR
	if((sockfd=socket(AF_LOCAL, SOCK_DGRAM, 0))<0) {

		perror("SERVER: Failed to open socket\n");
		exit(1);
	}

	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sun_family=AF_LOCAL;
	//server well-known sun_path
	strcpy(srvaddr.sun_path, "240411");

    unlink(srvaddr.sun_path);

	if(bind(sockfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {

		perror("SERVER: Failed to bind socket\n");
		exit(1);
	}
	printf("Server UNIX domain socket was open and binded\n\n");

	int recv;	
	int src_port;
	char src_addr[15];	
	char sendmsg[ETH_FRAME_LEN];
	char recvmsg[ETH_FRAME_LEN];
	time_t ticks;

	while(1) {

		//receive request from local ODR	
		printf("Waiting for request......\n\n");				
		msg_recv(sockfd, recvmsg, src_addr, &src_port);

		//get source node number of request
		for(index=0; index<10; index++) {

			if(strcmp(src_addr, vm_addr[index])==0) {
		
				recv=index+1;
				break;
			}
		}
		printf("Server at node VM%d received request from VM%d\n", node, recv);
		printf("Received message: %s\n\n", recvmsg);
		
		ticks=time(NULL);       
        snprintf(sendmsg, sizeof(sendmsg), "%.24s\r\n", ctime(&ticks));
        
        //send reply to local ODR
		msg_send(sockfd, vm_addr[recv-1], src_port, sendmsg, 0);
		printf("Server at node VM%d responded to request from VM%d\n\n", node, recv);
	}

	unlink(srvaddr.sun_path);
	return 0; 
}

