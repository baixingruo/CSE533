//
//  tcpechotimesrv.c
//  HW1
//
//  Created by Panchen Xue on 9/27/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "unpthread.h"

void echo_cli(FILE *fp, int sockfd, int pipe_port);

int main(int argc, char **argv) {

    int sockfd;
    int pipe_port;
    struct sockaddr_in servaddr;

    sscanf(argv[2], "%d", &pipe_port);

    sockfd=Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(55000);
    Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    if(connect(sockfd, (SA *) &servaddr, sizeof(servaddr))<0) {

		Write(pipe_port, "Server is not ready or is not reachable", 50);	
		exit(1);
    }

    echo_cli(stdin, sockfd, pipe_port);

    close(pipe_port);

    exit(0);
}

void echo_cli(FILE *fp, int sockfd, int pipe_port) {

    int maxfdp;
    int echo_flag;
    int read_count;
    fd_set rset;
    char buf[MAXLINE];
    
    echo_flag=0;
    FD_ZERO(&rset);

    while(1) {

		if(echo_flag==0) FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        maxfdp=max(fileno(fp), sockfd)+1;

        Select(maxfdp, &rset, NULL, NULL, NULL);

        //echo from server
        if(FD_ISSET(sockfd, &rset)) {

	    	if((read_count=Read(sockfd, buf, MAXLINE))==0) {

				if(echo_flag==1) {

		    		Write(pipe_port, "Server terminated", 20);
		    		return;
				}
            	else {

		    		Write(pipe_port, "Server terminated prematurely", 30);
		    		return;
				}
			}

        	Write(fileno(stdout), buf, read_count);
	    	Write(pipe_port, "User input is echoed back", 30);
  		}

        //user input
        if (FD_ISSET(fileno(fp), &rset)) {

            if((read_count=Read(fileno(fp), buf, MAXLINE))==0) {

                echo_flag=1;
				Write(pipe_port, "No more user input", 20);
                Shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
	    	}
	    
            Write(sockfd, buf, read_count);
	    	Write(pipe_port, "User input is forwarded to server", 30);
        }
    }

    return;
}

