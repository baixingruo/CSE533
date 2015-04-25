//
//  tcpechotimesrv.c
//  HW1
//
//  Created by Panchen Xue on 9/27/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include "unpthread.h"

static void *echo_serv(void *);
void str_echo(int);
static void *time_serv(void *);

int main(int argc, char **argv) {
    
    //echo socket
    int echo_lstnfd;
    int* echo_connfd;
    int echo_flag;
    const int echo_on=1;
    pthread_t echo_tid;
    
    struct sockaddr_in echo_servaddr;
    struct sockaddr* echo_cliaddr;
    socklen_t echo_addrlen;
    socklen_t echo_len;
    
    if((echo_lstnfd=socket(AF_INET, SOCK_STREAM, 0))<0) {
        
        perror("TCPECHOTIMESERV: socket failed to open");
        exit(1);
    }

    //set socket NON_BLOCKING option
    if((echo_flag=fcntl(echo_lstnfd, F_GETFL, 0))<0) {
        
        perror("TCPECHOTIMESERV: F_GETFL error");
        exit(1);
    }

    echo_flag|=O_NONBLOCK;

    if(fcntl(echo_lstnfd, F_SETFL, echo_flag)<0) {
        
        perror("TCPECHOTIMESERV: F_SETFL error");
        exit(1);
    }

    //set socket SO_REUSEADDR option
    if(setsockopt(echo_lstnfd, SOL_SOCKET, SO_REUSEADDR, &echo_on, sizeof(echo_on))<0) {
        
        perror("TCPECHOTIMESERV: failed to set socket option SO_REUSEADDR");
        exit(1);
    }    

    //bind echo socket and turn it into listening socket
    bzero(&echo_servaddr, sizeof(echo_servaddr));
    echo_servaddr.sin_family=AF_INET;
    echo_servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    echo_servaddr.sin_port=htons(55000);

    echo_addrlen=sizeof(echo_servaddr);

    if(bind(echo_lstnfd, (struct sockaddr *) &echo_servaddr, sizeof(echo_servaddr))<0) {
        
        perror("TCPECHOTIMESERV: bind error");
        exit(1);
    }
    if(listen(echo_lstnfd, LISTENQ)<0) {
        
        perror("TCPECHOTIMESERV: listen error");
        exit(1);
    }

    echo_cliaddr=malloc(echo_addrlen);

    printf("Echo socket option is all set and turned into listening socket\n");
       
    //time socket
    int time_lstnfd;
    int* time_connfd;
    int time_flag;
    const int time_on=1;
    pthread_t time_tid;
    
    struct sockaddr_in time_servaddr;
    struct sockaddr* time_cliaddr;
    socklen_t time_addrlen;
    socklen_t time_len;
    
    //set socket NON_BLOCKING option
    if((time_lstnfd=socket(AF_INET, SOCK_STREAM, 0))<0) {
        
        perror("TCPECHOTIMESERV: socket failed to open");
        exit(1);
    }

    if((time_flag=fcntl(time_lstnfd, F_GETFL, 0))<0) {
        
        perror("TCPECHOTIMESERV: F_GETFL error");
        exit(1);
    }

    time_flag|=O_NONBLOCK;

    if(fcntl(time_lstnfd, F_SETFL, time_flag)<0) {
        
        perror("TCPECHOTIMESERV: F_SETFL error");
        exit(1);
    }

    //set socket SO_REUSEADDR option
    if(setsockopt(time_lstnfd, SOL_SOCKET, SO_REUSEADDR, &time_on, sizeof(time_on))<0) {
        
        perror("TCPECHOTIMESERV: failed to set socket option SO_REUSEADDR");
        exit(1);
    }    

    //bind time socket and turn it into listening socket
    bzero(&time_servaddr, sizeof(time_servaddr));
    time_servaddr.sin_family=AF_INET;
    time_servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    time_servaddr.sin_port=htons(50000);

    time_addrlen=sizeof(time_servaddr);

    if(bind(time_lstnfd, (struct sockaddr *) &time_servaddr, sizeof(time_servaddr))<0) {
        
        perror("TCPECHOTIMESERV: bind error");
        exit(1);
    }
    if(listen(time_lstnfd, LISTENQ)<0) {
        
        perror("TCPECHOTIMESERV: listen error");
        exit(1);
    }

    time_cliaddr=Malloc(time_addrlen);

    printf("Time socket option is all set and turned into listening socket\n");

    printf("Server is waiting for connection\n");
    
    fd_set rset;
    int maxfdp;    

    FD_ZERO(&rset);

    while(1) {
        
		FD_SET(echo_lstnfd, &rset);        
		FD_SET(time_lstnfd, &rset);
        
        maxfdp=max(time_lstnfd, echo_lstnfd)+1;
        
        Select(maxfdp, &rset, NULL, NULL, NULL);

		printf("Server is listening\n");
        
		//echo service is requested        
		if(FD_ISSET(echo_lstnfd, &rset)) {
           
	    	printf("New echo service is requested\n");            

	    	echo_len=echo_addrlen;
            echo_connfd=malloc(sizeof(int));
            *echo_connfd=Accept(echo_lstnfd, echo_cliaddr, &echo_len);

	    	printf("New echo service is connected\n");
            
            echo_flag&=~O_NONBLOCK;
            if(fcntl(*echo_connfd, F_SETFL, echo_flag)<0) {
                    
                perror("TCPECHOTIMESERV: F_SETFL error");
                exit(1);
            }
            printf("Connected echo socket is set BLOCKING\n");

			//create new thread to handle echo service
            Pthread_create(&echo_tid, NULL, &echo_serv, echo_connfd);
	    	printf("New thread is created for echo service\n");
        }
        
		//time service is requested
        if(FD_ISSET(time_lstnfd, &rset)) {
            
	    	printf("New time service is requested\n"); 

            time_len=time_addrlen;
            time_connfd=malloc(sizeof(int));
            *time_connfd=Accept(time_lstnfd, time_cliaddr, &time_len);
	    	printf("New time service is connected\n");
            
            time_flag&=~O_NONBLOCK;
            if(fcntl(*time_connfd, F_SETFL, time_flag)<0) {
                
                perror("TCPECHOTIMESERV: F_SETFL error");
                exit(1);
            }
	    	printf("Connected time socket is set BLOCKING\n");

            //create new thread to handle echo service
            Pthread_create(&time_tid, NULL, &time_serv, time_connfd);
	    	printf("New thread is created for time service\n"); 
        }
    }
}

static void *echo_serv(void *arg) {
    
    printf("This is the new window for echo service\n"); 
    
    int sockfd;
    sockfd=*((int*)arg);
    free(arg);

    Pthread_detach(pthread_self());
    
    str_echo(sockfd);
       
    Close(sockfd);
    return NULL;
}

void str_echo(int sockfd) {

    ssize_t read_count;
    char buf[MAXLINE];
    
    again:
        
    while((read_count=read(sockfd, buf, MAXLINE))>0) Write(sockfd, buf, read_count);

    if(read_count<0 && errno==EINTR) goto again;
    else if(read_count<0) {
            
        perror("TCPECHOTIMESERV: client terminated");
        return;
    }

    printf("Echo service is terminated\n");
    return;
} 

static void *time_serv(void *arg) {
    
    printf("This is the new window for time service\n"); 

    Pthread_detach(pthread_self());
    
    int sockfd=*((int*)arg);
    free(arg);
    time_t ticks;
    char buff[MAXLINE];

    while(1) {

        if(Readable_timeo(sockfd, 5)==0) {

	    ticks=time(NULL);        
            snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
        
            Write(sockfd, buff, strlen(buff));

	    printf("Current time is sent to client\n");
	}
	else break;
    }
    
    printf("This time service is terminated\n");

    Close(sockfd);
    return NULL;
}

