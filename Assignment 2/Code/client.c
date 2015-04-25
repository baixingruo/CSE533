//
//  client.c
//  HW2
//
//  Created by Panchen Xue on 10/31/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/select.h>
#include "unpthread.h"
#include "unpifiplus.h"
#include "unprtt.h"
#include "LinkList.h"

#define MAXIMUM_SOCKET 10

struct socket_info {

    struct in_addr ip_addr;
    struct in_addr mask;
};

struct hdr {

    uint32_t seq;
    uint32_t ts;
    uint32_t recv_window;
};

void *print_out(void *);

struct node *header;

int data_flag=0;

int process_flag=0;

int printed=0;

int main(int argc, char **argv) {
  
    FILE *fp;
    char srv_ipaddr[50];
    char filename[MAXLINE];
    int port_number;
    int recv_window;
    int seed;
    double loss_rate;
    int read_rate;

    header=(struct node *) malloc(sizeof(struct node));

    if((fp=fopen("client.in", "r"))==NULL) {

        perror("SERVER: Can't open file: client.in\n");
        exit(1);
    }
    printf("\nFile client.in is open\n");

    if(fscanf(fp, "%s %d %s %d %d %lf %d", srv_ipaddr, &port_number, filename, &recv_window, &seed, &loss_rate, &read_rate)<0) {

        perror("SERVER: Can't read data from file: client.in\n");
        exit(1);
    }
    printf("  server IP address : %s\n", srv_ipaddr);
    printf("  server port number: %d\n", port_number);
    printf("  filename: %s\n", filename);
    printf("  receiving sliding-window size: %d\n", recv_window);
    printf("  random generator seed value: %d\n", seed);
    printf("  probability p of datagram loss: %f\n", loss_rate);
    printf("  the mean u of client read rate, in milliseconds: %d\n\n", read_rate);

    srand(seed);

    struct sockaddr_in srvaddr, cliaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    bzero(&cliaddr, sizeof(srvaddr));
    
    srvaddr.sin_addr.s_addr=inet_addr(srv_ipaddr);
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_port = htons(port_number);

    int socket_no=0;
    const int on=1;
    struct ifi_info *ifi, *ifi_head;
    struct sockaddr_in *sa;
    struct socket_info sockets[MAXIMUM_SOCKET];
    int max_mask=0;
    int max_mask_len=0;
    socklen_t len;
    int i;

    if((ifi_head=get_ifi_info_plus(AF_INET, 1))==NULL) {

        perror("CLIENT: GET_IFI_INFO_PLUS error\n");
        exit(1);
    }
    printf("List of client IP addresses\n");

    for(ifi=ifi_head; ifi!=NULL; ifi=ifi->ifi_next) {

        sa=(struct sockaddr_in *) ifi->ifi_addr;
        sockets[socket_no].ip_addr.s_addr=ntohl(sa->sin_addr.s_addr);
        printf("  IP addr: %s\n", Sock_ntop_host((SA *) sa, sizeof(*sa)));

        sa=(struct sockaddr_in *) ifi->ifi_ntmaddr;
        sockets[socket_no].mask.s_addr=ntohl(sa->sin_addr.s_addr);
        printf("  network mask: %s\n\n", Sock_ntop_host((SA *) sa, sizeof(*sa)));

        socket_no++;
    }

    for(i=0; i<socket_no; i++) {

        if(sockets[i].ip_addr.s_addr==ntohl(srvaddr.sin_addr.s_addr)) {

            strcpy(srv_ipaddr, "127.0.0.1");

            if(inet_pton(AF_INET, srv_ipaddr, &srvaddr.sin_addr)<0 || inet_pton(AF_INET, srv_ipaddr, &cliaddr.sin_addr)<0) {

                perror("CLIENT: Function inet_pton() error\n");
                exit(1);
            }
            
            break;
        }
        else if((ntohl(srvaddr.sin_addr.s_addr) & sockets[i].mask.s_addr)
        		==(sockets[i].ip_addr.s_addr & sockets[i].mask.s_addr)) {

            max_mask=i+1;
            break;     
        }
    }

    int sockfd;

    if((sockfd=socket(AF_INET, SOCK_DGRAM, 0))<0) {

    	perror("CLIENT: Socket open error\n");
    	exit(1);
    }
    printf("Client socket is open\n");

    if(strcmp(srv_ipaddr, "127.0.0.1")==0 || max_mask!=0) {

        if(setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on))<0) {

        	perror("SERVER: Set socket option SO_DONTROUTE failed\n");
        	exit(1);
        }
        printf("SO_DONTROUTE option is set for client socket\n\n");

        printf("Server is local\n\n");

        if(strcmp(srv_ipaddr, "127.0.0.1")==0) {

            cliaddr.sin_family=AF_INET;
            cliaddr.sin_port=htons(0);
        }
        else {

            cliaddr.sin_addr.s_addr=htonl(sockets[max_mask-1].ip_addr.s_addr);
            cliaddr.sin_family=AF_INET;
            cliaddr.sin_port=htons(0);
        }
    }
    else {

        printf("Server is not local\n\n");

        cliaddr.sin_addr.s_addr=htonl(sockets[1].ip_addr.s_addr);
        cliaddr.sin_family=AF_INET;
        cliaddr.sin_port=htons(0);
    }

    printf("Server IP address is: %s\n", inet_ntoa(srvaddr.sin_addr));
    printf("Client IP address is: %s\n", inet_ntoa(cliaddr.sin_addr));
  
    if(bind(sockfd, (SA *) &cliaddr, sizeof(cliaddr))<0) {

        perror("CLIENT: socket bind error\n");
        exit(1);
    }

    len=sizeof(cliaddr);

    if(getsockname(sockfd, (SA *) &cliaddr, &len)<0) {

        perror("CLIENT: Function getsockname error\n");
        exit(1);
    }

    printf("After binded, client IP address: %s\n", Sock_ntop_host((SA *) &cliaddr, sizeof(cliaddr)));
    printf("After binded, client port number: %d\n", ntohs(cliaddr.sin_port));

    if(connect(sockfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {

        perror("CLIENT: Server connection failed\n");
	   	exit(1);
    }

    if(getpeername(sockfd, (SA *) &srvaddr, &len)<0) {

        perror("CLIENT: Function getpeername() error\n");
        exit(1);
    }
    printf("After connection, server IP address: %s\n", Sock_ntop_host((SA *) &srvaddr, sizeof(srvaddr)));
    printf("After connection, server port number: %d\n\n", ntohs(srvaddr.sin_port));

    fd_set rset;
    FD_ZERO(&rset);
    int sel;

    struct timeval tv;
    tv.tv_sec=3;
    tv.tv_usec=0;

    while(1){

        if((((double) rand())/RAND_MAX)>loss_rate) {

            if(send(sockfd, filename, sizeof(filename), 0)<0) {

                perror("SERVER: Unable to send file name to server\n");
                exit(1);
            }
            printf("Client sent request filename to server\n");
        }
        else printf("Sending request filename was lost by simulation\n");

        FD_SET(sockfd, &rset);

        if((sel=select(sockfd+1, &rset, NULL, NULL, &tv))<0) {
	
	    	if(errno==EINTR) continue;
	        else {

                perror("CLIENT: Read error from client\n");
	  	        exit(1);
	    	}
        }
        else if(sel==0) {

            printf("Timeout before hearing from server ephemeral port number\n");
            
            continue;
        }
        else{

            if(FD_ISSET(sockfd, &rset)) {

                if(recv(sockfd, &port_number, sizeof(port_number), 0)<0) {

                    perror("CLIENT: Unable to receive new port number from server\n");
                    exit(1);
                }

                printf("New server port number: %d\n", ntohs(port_number));
                break;
            }
        }
    }

    srvaddr.sin_port=htons(port_number);
    
    if(connect(sockfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {

        perror("CLIENT: Server reconnection failed\n");
        exit(1);

    }
    printf("Client re-connected to server new socket\n");

    recv_window=htons(recv_window);

    if(send(sockfd, &recv_window, sizeof(recv_window), 0)<0) {

        perror("CLIENT: Failed to send advertised window size to server\n");
        exit(1);
    }
    printf("Client sent advertised window size to server\n\n");

    struct hdr send_hdr, recv_hdr;
    char recv_data[MAXLINE+1];
    int read_count;
    pthread_t time_tid;
   
    printf("File transmission starts\n\n");

    Pthread_create(&time_tid, NULL, &print_out, &read_rate);
    printf("New thread was created for received file data print out\n\n"); 

    while(1){

        if(recv(sockfd, &recv_hdr, sizeof(recv_hdr), 0)<0) {

            perror("CLIENT: Can't read data from server using recv() function\n");
            exit(1);  
        }

        if(ntohl(recv_hdr.seq)==0) {

            if((((double) rand())/RAND_MAX)>loss_rate) {

                printf("Received advertised window size update request from server\n");

                if(((double) rand())/RAND_MAX>loss_rate) {

                    send_hdr.seq=htonl(1);
                    send_hdr.ts=htonl(0);
                    send_hdr.recv_window=recv_window-htons(LinkListLength(header));

                    if(ntohl(send_hdr.recv_window)==0) printf("Client receive buffer locks\n");

                    if(send(sockfd, &send_hdr, sizeof(send_hdr), 0)<0) {

                        perror("CLIENT: Unable to send advertised window size to server\n");
                        exit(1);
                    }
                    printf("Receive window size is sent to server\n");
                }
                else printf("Sending advertised window size is lost by simulation\n");
            }
            else printf("Incoming advertised window size update request is lost by simulation\n"); 
        }
        else {

            if((read_count=recv(sockfd, recv_data, sizeof(recv_data), 0))<0) {

                perror("CLIENT: Can't read data from server using recv() function\n");
                exit(1); 
            }

            recv_data[read_count]=0;

            if((((double) rand())/RAND_MAX)>loss_rate) {

                if(ntohl(recv_hdr.seq)>printed) InsertLinkList(header, ntohl(recv_hdr.seq), recv_data);

                printf("Received datagram sequence number: %d\n", ntohl(recv_hdr.seq));

                if(ntohl(recv_hdr.ts)==0 && NextAck(header, printed)==ntohl(recv_hdr.seq)+1) {

                    if(data_flag==0) printf("All request file data was received\n\n");

                    data_flag=1;

                    send_hdr.seq=htonl(0);
                }
                else send_hdr.seq=htonl(NextAck(header, printed));
   
                if((((double) rand())/RAND_MAX)>loss_rate) {

                    send_hdr.ts =recv_hdr.ts;
                    send_hdr.recv_window=recv_window-htons(LinkListLength(header));

                    if(ntohl(send_hdr.recv_window)==0) printf("Client receive buffer locks\n");

                    if(send(sockfd, &send_hdr, sizeof(send_hdr), 0)<0) {

                        perror("CLIENT: Unable to send ACK to server\n");
                        exit(1);
                    }
                    printf("ACK %d is sent to server\n\n", ntohl(send_hdr.seq));

                    if(data_flag==1) break;
                }
                else printf("Sending ACK is lost by simulation\n");
            }
            else printf("Incoming data is lost by simulation\n");

            bzero(recv_data, sizeof(recv_data));
        }                
    }

    if(recv(sockfd, &i, sizeof(int), 0)<0) {

        perror("Receive error\n");
        exit(1);
    }
    printf("Client confirmed on the termination of file data transfer\n\n");

    struct timeval timer;
    timer.tv_sec=1;
    timer.tv_usec=0;

    while(1) {

        if(select(0, NULL, NULL, NULL, &timer)==0) {

          	if(process_flag==1) break;
        }
    }

    printf("Print process is finished, client is closing\n\n");

    return 0;
}

void *print_out(void *arg) {
    
    printf("This is the new thread window for print out\n\n");

    Pthread_detach(pthread_self());

    int mean;
    mean=*((int *) arg);

    double sleep_time;
    struct timeval timer;
    timer.tv_sec=0;
    timer.tv_usec=0;

    while(1) {

        sleep_time=(-1)*mean*log(((double) rand())/RAND_MAX);
        printf("Client fell asleep for %f milliseconds\n\n", sleep_time);

        timer.tv_usec=sleep_time*1000;

        if(select(1, NULL, NULL, NULL, &timer)==0) {

            printf("Client process is activated to print received file data\n\n");

            while(header->next!=NULL && header->next->seq==printed+1) {

                printf("%s", header->next->recvline);

                struct node *temp=header->next;
                header->next=temp->next;

                free(temp);

                printed++;
            }
        }

        if(data_flag==1) {

            printf("\nAll request file data was printed\n");
            break;
        }
    }

    process_flag=1;

    printf("Print process is closing\n");

    return NULL;    
}

