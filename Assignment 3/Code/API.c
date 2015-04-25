//
//  API.c
//  HW3
//
//  Created by Panchen Xue on 11/10/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include "API.h"

void msg_send(int fd, char* dst_addr, int dst_port, char* sendmsg, int flag) {

	//send message
	char data[ETH_FRAME_LEN];
	int cnt;
     
	//format parameters into single char sequence
    if((cnt=sprintf(data, "%s %d %s %d", dst_addr, dst_port, sendmsg, flag))<=0) {

		perror("MSG_SEND: Failed to format parameters into a single data sequence\n");
		exit(1);
	}
	printf("Parameters were formatted into one single date sequence\n");
	
	struct sockaddr_un odraddr;
	odraddr.sun_family=AF_LOCAL;
	//ODR well-known path name
    strcpy(odraddr.sun_path, "114042");

	//send char sequence to local ODR
    if(sendto(fd, data, cnt, 0, (SA *) &odraddr, sizeof(odraddr))<0) {

		perror("MSG_SEND: Failed to send data sequence to local ODR\n");
		exit(1);
	}	
	printf("Data sequence of %d characters was sent to local ODR\n\n", cnt);
  
	return ;
}

void msg_recv(int fd, char* recvmsg, char* src_addr, int* src_port) {

	//received message
	char data[ETH_FRAME_LEN+1];	
	int cnt;
     
	//receive char sequence from local ODR
	if((cnt=recvfrom(fd, data, ETH_FRAME_LEN, 0, NULL, NULL))<0) {

		perror("MSG_RECV: Failed to receive data sequence from local ODR\n");
		exit(1);
	}
	printf("Data sequence of %d characters was received from local ODR\n", cnt);

	//char sequence is decomposed into three parameters
	sscanf(data, "%s %s %d", recvmsg, src_addr, src_port);
	printf("Data sequence was decomposed into three parameters\n\n");
     	
	return ;
}

