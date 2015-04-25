//
//  client.c
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

	printf("\nWELCOME! THIS IS CLIENT SERVICE\n\n");

	struct hwa_info *hwa;
    struct sockaddr *sa;
    //node index
    int node;
	int index;

	if((hwa=get_hw_addrs())==NULL) {

		perror("CLIENT: Failed to get interface information for this node\n");
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
	char filename[15];
	struct sockaddr_un cliaddr;

	//UNIX domain socket to communicate with local ODR
	if((sockfd=socket(AF_LOCAL, SOCK_DGRAM, 0))<0) {

		perror("CLIENT: Failed to open UNIX domain socket\n");
		exit(1);
	}

	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sun_family=AF_LOCAL;

	strcpy(filename, "temp.XXXXXX");

	//get unique temporary file name
	if(mkstemp(filename)<0) {

		perror("CLIENT: Failed to create a new unique file\n");
		exit(1);
	}
	printf("New temporary file: %s\n", filename);
	
	strcpy(cliaddr.sun_path, filename);
	
	unlink(cliaddr.sun_path);

	if(bind(sockfd, (SA *) &cliaddr, sizeof(cliaddr))<0) {

		perror("CLIENT: Failed to bind socket\n");
		exit(1);
	}
	printf("Client UNIX domain socket was open and binded\n\n");

	int send, recv;
	int src_port;
	char src_addr[15];
	char sendmsg[3]="HI";
	char recvmsg[ETH_FRAME_LEN+1];
	time_t ticks;
	char time_buff[MAXLINE];
	
	fd_set rset;
	struct timeval tv;
	int sel;

	while(1) {

    	printf("Please choose a VM as the server node (1 ~ 10): \n");
    	
        if(scanf("%d", &send)<0) {
        
        	perror("CLIENT: Failed to receive user input\n");
        	exit(1);
        }

        //send request to local ODR
		msg_send(sockfd, vm_addr[send-1], 5000, sendmsg, 0);
        printf("\nClient at VM%d sent request to server at VM%d\n\n", node, send);
  	
		//receive reply from local ODR
		printf("Waiting for replay......\n\n");
		
		SELECT:
		
		//attempt to read is backed up by a 3 seconds timeout
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		tv.tv_sec=3;
    	tv.tv_usec=0;
		
		if((sel=select(sockfd+1, &rset, NULL, NULL, &tv))<0) {
		
			perror("CLIENT: Failed to read from local ODR\n");
			exit(1);
		}
		//timeout
		else if(sel==0) {	
            
            printf("Client at VM%d timeout on response from VM%d\n", node, send);
            
            //retransmit the message and set force discovery flag
            msg_send(sockfd, vm_addr[send-1], 5000, sendmsg, 1);
            printf("Client retransmitted message out, with the flag set to force a route rediscovery\n\n");
            
            goto SELECT;
        }
        else{
		
			msg_recv(sockfd, recvmsg, src_addr, &src_port);

			//get source node index of reply
			for(index=0; index<10; index++) {

				if(strcmp(src_addr, vm_addr[index])==0) {
		
					recv=index+1;
					break;
				}
			}

			//get time stamp
			ticks=time(NULL);       
        	snprintf(time_buff, sizeof(time_buff), "%.24s\r\n", ctime(&ticks));
        
			printf("Client at node VM%d received response from VM%d: <%s>\n", node, recv, time_buff);
			printf("Received message: %s\n\n", strcmp(recvmsg, "HI")==0?"Routing information is obtained":time_buff);
		}
	}

	unlink(cliaddr.sun_path);
	return 0; 
}

