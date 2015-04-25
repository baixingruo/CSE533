//
//  time_cli.c
//  HW1
//
//  Created by Panchen Xue on 9/27/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "unpthread.h"

int main(int argc, char** argv) {
    
    int sockfd;
    int pipe_port;
    int read_count;
    struct sockaddr_in servaddr;
    char recvline[MAXLINE];    

    sscanf(argv[2], "%d", &pipe_port);

    printf("Following is the child time process\n"); 
    
    if((sockfd=socket(AF_INET, SOCK_STREAM, 0))<0) {
        
		Write(pipe_port, "TIME_CLI: socket failed to open", 40);        
        exit(1);
    }
    printf("Time socket is created\n"); 
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(50000);
    if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr)<0) {
        
		Write(pipe_port, "TIME_CLI: inet_pton error", 40);          
        exit(1);
    }
    
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
        
        Write(pipe_port, "Server is not working or is not reachable", 50);
		exit(1);
    }

    printf("Time socket is connected to server\n");
        
    while((read_count=read(sockfd, recvline, MAXLINE))>0) {
        
        recvline[read_count]=0;
        
        if(fputs(recvline, stdout)<0) {
            
            Write(pipe_port, "TIME_CLI: fputs error", 30);
	    	perror("TIME_CLI: fputs error");
            exit(1);
        }
        
        Write(pipe_port, "Received current time from server", 40);
    }
    
    if(read_count<0) {
        
		Write(pipe_port, "Server crashed", 20);        
		perror("TIME_CLI: server crashed");
        exit(1);
    }
    
    printf("TIME_CLI: server teminated\n");
    Write(pipe_port, "Server teminated", 20);
    close(pipe_port);
    exit(0);
}

