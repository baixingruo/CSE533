//
//  server.c
//  HW2
//
//  Created by Panchen Xue on 10/9/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/select.h>
#include "unpifiplus.h"
#include "unprtt.h"
#include "CirQueue.h"
#include "LinkList.h"

#define MAXIMUM_SOCKET 10

//IP and mask address of each server interface
struct socket_info {

    int sockfd;
    struct in_addr ip_addr;
    struct in_addr mask;
    struct in_addr subnet;
};

//header of each datagram sent to client
struct hdr {

    uint32_t seq;
    uint32_t ts;
    int recv_window;
};

sigjmp_buf jmpbuf_1, jmpbuf_2;

//signal handler
void sig_chld(int signo);

void handshake_data_alrm(int signo);

void file_data_alrm(int signo);

void buffer_alrm(int signo);

//facility function
int three_min(int first, int second, int third);

//main function
int main(int argc, char **argv) {

	//server reads port number and send window arguments from server.in file
    FILE *fp;
    int port_number;
    int send_window;

    if((fp=fopen("server.in", "r"))==NULL) {

		perror("SERVER: Can't open file: server.in\n");
		exit(1);
    }
    printf("\nFile server.in is open\n");

    if(fscanf(fp, "%d %d", &port_number, &send_window)<0) {

		perror("SERVER: Can't read data from file: server.in\n");
		exit(1);
    }
    printf("  server port number: %d\n", port_number); 
    printf("  maximum send sliding-window size: %d\n\n", send_window);
	
	//server gets all its IP addresses and bind each IP address to a seperate UDP socket
    int sockfd;
    int socket_no=0;
    const int on=1;
    struct ifi_info *ifi, *ifi_head;
    struct sockaddr_in *sa;
    struct socket_info sockets[MAXIMUM_SOCKET];

    if((ifi_head=get_ifi_info_plus(AF_INET, 1))==NULL) {

		perror("SERVER: GET_IFI_INFO_PLUS error\n");
		exit(1);
    }
    printf("List of server IP addresses\n");

    for(ifi=ifi_head; ifi!=NULL; ifi=ifi->ifi_next) {

		if((sockfd=socket(AF_INET, SOCK_DGRAM, 0))<0) {

	    	perror("SERVER: Socket open error\n");
	    	exit(1);
		}

		sockets[socket_no].sockfd=sockfd;

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0) {

	    	perror("SERVER: Set socket option SO_REUSEADDR failed\n");
	    	exit(1);
		}

		sa=(struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family=AF_INET;
		sa->sin_port=htons(port_number);

		if(bind(sockfd, (SA *) sa, sizeof(*sa))<0) {

	    	perror("SERVER: Socket bind error\n");
	    	exit(1);
		}

		sockets[socket_no].ip_addr.s_addr=ntohl(sa->sin_addr.s_addr);
		printf("  IP addr: %s\n", Sock_ntop_host((SA *) sa, sizeof(*sa)));

		sa=(struct sockaddr_in *) ifi->ifi_ntmaddr;
		sockets[socket_no].mask.s_addr=ntohl(sa->sin_addr.s_addr);
		printf("  network mask: %s\n", Sock_ntop_host((SA *) sa, sizeof(*sa)));
			
		sockets[socket_no].subnet.s_addr=sockets[socket_no].mask.s_addr & sockets[socket_no].ip_addr.s_addr;
		sa->sin_addr.s_addr=htonl(sockets[socket_no].subnet.s_addr);
		printf("  subnet addr: %s\n\n", Sock_ntop_host((SA *) sa, sizeof(*sa)));

		socket_no++;
    }

    printf("Sockets are all open and binded with each server IP address\n");

	//server sets signal handler for SIGCHLD
    if(signal(SIGCHLD, sig_chld)==SIG_ERR) {
            
        perror("SERVER: unable to catch SIGCHLD signal\n");
        exit(1);
    }
    printf("Signal handler is set for SIGCHLD\n");
    printf("Server is listening on all IP addresses\n\n");

	//server is listening to all its sockets and waits for client requests
    int childfd;
    int ephemeral_port;
    socklen_t len;
    int maxfdp=0;
    char filename[MAXLINE];
    fd_set rset;
    pid_t pid;
    int i, j;

	//duplicate connection request from same client
    struct node *connected[socket_no];

    for(i=0; i<socket_no; i++) connected[i]=(struct node *) malloc(sizeof(struct node));

    FD_ZERO(&rset);

    while(1) {

		for(i=0; i<socket_no; i++) {

	    	FD_SET(sockets[i].sockfd, &rset);
	    	maxfdp=sockets[i].sockfd>maxfdp?sockets[i].sockfd:maxfdp;
		}

		maxfdp++;

		if(select(maxfdp, &rset, NULL, NULL, NULL)<0) {
	
	    	if(errno==EINTR) continue;
	    	else {

				perror("SERVER: Read error from client\n");
				exit(1);
	    	}
		}

		for(i=0; i<socket_no; i++) {

			//there is a client request on selected server socket
	    	if(FD_ISSET(sockets[i].sockfd, &rset)) {

				struct sockaddr_in cliaddr;
				len=sizeof(cliaddr);
				bzero(&cliaddr, sizeof(struct sockaddr_in));

				//server gets client address information and its request file name
				if(recvfrom(sockets[i].sockfd, filename, MAXLINE, 0, (SA *) &cliaddr, &len)<0) {

		    		perror("SERVER: Read error from client\n");
		    		exit(1);
				}

				if(DuplicateNode(connected[i], sock_get_port((SA *) &cliaddr, sizeof(cliaddr)), 
						Sock_ntop_host((SA *) &cliaddr, sizeof(cliaddr)))==1) {

					printf("Duplicate connection request from client that are already connected\n");
					continue;
				}
				else {

					InsertLinkList(connected[i], sock_get_port((SA *) &cliaddr, sizeof(cliaddr)), 
							Sock_ntop_host((SA *) &cliaddr, sizeof(cliaddr)));

					struct in_addr temp;
					temp.s_addr=htonl(sockets[i].ip_addr.s_addr);
					printf("New connect request on IP address: %s\n", inet_ntoa(temp));

					printf("Connect request from client IP address: %s\n", Sock_ntop_host((SA *) &cliaddr, sizeof(cliaddr)));
					printf("Request file name: %s\n", filename);
				}

				//server forks a child process to handle the file transmission task with client
				if((pid=fork())<0) {
                
            		perror("SERVER: fork failed");
            		exit(1);
        		}

				//server child process
				if(pid==0) {

					printf("Server forked a child process to handle new client\n\n");

					//server child closes other inherited sockets
					for(j=0; j<socket_no; j++) {

						if(j!=i) {

							if(close(sockets[j].sockfd)<0) {

								perror("SERVER: Socket close error\n");
								exit(1);
							}
						}
					}
					printf("All the other inherited sockets were closed\n\n");

					//server determines whether this client is local to himself and opens a send socket for file transmission
					//with client
		    		if(i==0 || (sockets[i].mask.s_addr & ntohl(cliaddr.sin_addr.s_addr))
		    				==sockets[i].subnet.s_addr) {
		    
		    			printf("Client host is local to server\n\n");

						if(setsockopt(sockets[i].sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on))<0) {

	    		    		perror("SERVER: Set socket option SO_DONTROUTE failed\n");
	    		    		exit(1);
						}
			
						if((childfd=socket(AF_INET, SOCK_DGRAM, 0))<0) {

	    		    		perror("SERVER: Child socket open error\n");
	    		    		exit(1);
						}
						printf("Child socket is open\n");

						if(setsockopt(childfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on))<0) {

	    		    		perror("SERVER: Set child socket option SO_DONTROUTE failed\n");
	    		    		exit(1);
						}
	    	    	}
		    		else {

						printf("Client host is NOT local to server\n");

						if((childfd=socket(AF_INET, SOCK_DGRAM, 0))<0) {

	    		    		perror("SERVER: Child socket open error\n");
	    		    		exit(1);
						}
						printf("Child socket is open\n");
		    		}

					//server binds its send socket, gets ephemeral port of this socket, and connect to the client socket
	  	    		struct sockaddr_in childaddr;
		    		bzero(&childaddr, sizeof(struct sockaddr_in));
		    		childaddr.sin_addr.s_addr=htonl(sockets[i].ip_addr.s_addr);
		    		childaddr.sin_family=AF_INET;
		    		childaddr.sin_port=htons(0);

		    		if(bind(childfd, (SA *) &childaddr, sizeof(childaddr))<0) {

	    				perror("SERVER: Child socket bind error\n");
	    				exit(1);
		    		}

		    		len=sizeof(childaddr);

		    		if(getsockname(childfd, (SA *) &childaddr, &len)<0) {

						perror("SERVER: Function getsockname() error\n");
	    				exit(1);
		    		}
		    		printf("  IP address: %s\n", Sock_ntop_host((SA *) &childaddr, sizeof(childaddr)));

		    		ephemeral_port=htons(childaddr.sin_port);
		    		printf("  Ephemeral port number: %d\n\n", ntohs(ephemeral_port));

		    		if(connect(childfd, (SA *) &cliaddr, sizeof(cliaddr))<0) {

						perror("SERVER: Child socket connection error\n");
	    				exit(1);
		    		}
		    		printf("Child socket is connected to client\n");

					//three-way handshake with client
		    		int recv_window;
		    		struct rtt_info rttinfo;
		    		int rttinit=0;

		    		if(rttinit==0) {

			    		rtt_init(&rttinfo);
			    		rttinit=1;
			    		rtt_d_flag=1;
					}

		    		if(signal(SIGALRM, handshake_data_alrm)==SIG_ERR) {
            
        				perror("SERVER: Unable to catch SIGALRM signal\n");
        				exit(1);
    		    	}
    		    	printf("Signal handler is set for SIGALRM\n\n");

		    		rtt_newpack(&rttinfo);

		    		if(sendto(sockets[i].sockfd, &ephemeral_port, sizeof(ephemeral_port), 0, (SA *) &cliaddr, sizeof(cliaddr))<0) {

						perror("SERVER: Unable to send ephemeral port number to client\n");
            			exit(1);
		    		}
		    		printf("Server sent ephemeral port number to client\n");

		    		AckAlarmAgain:

		    		alarm(rtt_start(&rttinfo));

		    		if(sigsetjmp(jmpbuf_1, 1)!=0) {

						printf("Timeout before hearing from client advertised window size\n");

						if(rtt_timeout(&rttinfo)<0) {

			    			perror("SERVER: No response from client, giving up\n");
			    			rttinit=0;
			    			errno=ETIMEDOUT;
			    			exit(1);
						}

						if(sendto(sockets[i].sockfd, &ephemeral_port, sizeof(ephemeral_port), 0, (SA *) &cliaddr, sizeof(cliaddr))<0 || 
			    			send(childfd, &ephemeral_port, sizeof(ephemeral_port), 0)<0) {

			   				perror("SERVER: Unable to send ephemeral port number to client\n");
            		    	exit(1);
		    			}
		    			printf("Server re-sends ephemeral port number to client\n");

						goto AckAlarmAgain;
		    		}

		    		if(recv(childfd, &recv_window, sizeof(recv_window), 0)<0) {

						perror("SERVER: Unable to receive advertised window size from client\n");
            			exit(1);
		    		}

		    		recv_window=ntohs(recv_window);
		    		printf("Client advertised receive window size is %d\n", recv_window);

		    		alarm(0);

		    		DeleteSelectedNode(connected[i], sock_get_port((SA *) &cliaddr, sizeof(cliaddr)), 
									Sock_ntop_host((SA *) &cliaddr, sizeof(cliaddr)));
		    		
		    		if(close(sockets[i].sockfd)<0) {

		    			perror("SERVER: Socket close error\n");
		    			exit(1);
		    		}

		    		rttinit=0;

		    		FILE *file;	    

		    		if((file=fopen(filename, "r"))==NULL) {

						perror("SERVER: Can't open request file\n");
						exit(1);
    		    	}
		    		printf("Request file %s is open\n\n", filename);
	    
					//server send buffer
		    		struct CirQueue *send_buffer=(struct CirQueue *) malloc(sizeof(struct CirQueue));
    		    	InitCirQueue(send_buffer, send_window);

					//the number of chars each datagram can send
		    		int packet_capacity=(512-2*sizeof(uint32_t)-sizeof(int))/sizeof(char);

		    		int sending_window=0;
		    		int cwnd=1, ssh=65535;
		    		int congestion_avoid_count=0;
		    		int more_to_send;
		    		int received=0;
		    		uint32_t sequence_number=0;
		    		uint32_t head=1;
		    		uint32_t measure_rtt;

		    		struct data localcopy;
		    		struct hdr send_hdr, recv_hdr;

		    		fd_set fdset;
		    		sigset_t sigset_alrm, sigset_empty;

		    		printf("File transmission starts\n\n");

					//file transmission starts and keeps going until the end of file
		    		while(1) {

						if(rttinit==0) {

			    			rtt_init(&rttinfo);
			    			rttinit=1;
			    			rtt_d_flag=1;
						}			
			
						//if client receive window is full			
						if(recv_window==0) {

			    			printf("Client receive buffer locks\n");

			    			FD_ZERO(&fdset);

			    			if(sigemptyset(&sigset_empty)<0 || sigemptyset(&sigset_alrm)<0) {

								perror("SERVER: Unable to empty signal set\n");
								exit(1);
			    			}

			    			if(sigaddset(&sigset_alrm, SIGALRM)<0) {

								perror("SERVER: Unable to put SIGALRM to alarm signal set\n");
								exit(1);
			    			}

			    			if(sigprocmask(SIG_BLOCK, &sigset_alrm, NULL)<0) {

								perror("SERVER: Unable to mask alarm signal set\n");
								exit(1);
			    			}

			    			if(signal(SIGALRM, buffer_alrm)==SIG_ERR) {
            
        						perror("SERVER: Unable to catch SIGALRM signal\n");
        						exit(1);
    			    		}

			    			while(1) {

								alarm(3);
								FD_SET(childfd, &fdset);

								printf("Server is waiting for client to annouce new advertised window size\n");
	
								if(pselect(childfd+1, &fdset, NULL, NULL, NULL, &sigset_empty)<0) {

				    				if(errno==EINTR) {

										send_hdr.seq=htonl(0);
			    						send_hdr.ts=htonl(0);

			    						if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

					    					perror("SERVER: Can't send data to client using send() function\n");
					    					exit(1);
			    						}
			    						printf("Server sent advertised window size update request to client\n");

										continue;
				    				}
				    				else {

										perror("SERVER: Function pselect() error\n");
										exit(1);
				    				}
								}

								if(recv(childfd, &recv_hdr, sizeof(recv_hdr), 0)<0) {

				    				perror("SERVER: Can't read data from client using recv() function\n");
				    				exit(1);
			        			}

			        			printf("Updated new advertised window size: %d\n", ntohs(recv_hdr.recv_window));

			    				if((recv_window=ntohs(recv_hdr.recv_window))>0) break;
			    			}
						}

						sending_window=three_min(cwnd, send_window, recv_window);
						printf("Server sending window size: %d\n\n", sending_window);

						if(sending_window==send_window) printf("Server send window locks\n\n");

						rtt_newpack(&rttinfo);

						if(IsEmptyCirQueue(send_buffer)==0) {

							if(sending_window<=CirQueueLength(send_buffer)) {

								for(j=0; j<sending_window; j++) {

									send_hdr.seq=htonl(send_buffer->base[send_buffer->front+j].seq);
			    					send_hdr.ts=htonl(send_buffer->base[send_buffer->front+j].ts);

			    					if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

										perror("SERVER: Can't send data to client using send() function\n");
										exit(1);
			    					}

			    					if(send(childfd, send_buffer->base[send_buffer->front+j].sendline, 
			    							sizeof(send_buffer->base[send_buffer->front+j].sendline), 0)<0) {

			    						perror("SERVER: Can't send data to client using send() function\n");
										exit(1);
			    					}

			    					printf("Datagram with sequence number %d was re-sent to client\n", send_buffer->base[send_buffer->front+j].seq);
								}
							}
							else {

								for(j=0; j<CirQueueLength(send_buffer); j++) {

									send_hdr.seq=htonl(send_buffer->base[send_buffer->front+j].seq);
			    					send_hdr.ts=htonl(send_buffer->base[send_buffer->front+j].ts);

			    					if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

										perror("SERVER: Can't send data to client using send() function\n");
										exit(1);
			    					}

			    					if(send(childfd, send_buffer->base[send_buffer->front+j].sendline, 
			    							sizeof(send_buffer->base[send_buffer->front+j].sendline), 0)<0) {

			    						perror("SERVER: Can't send data to client using send() function\n");
										exit(1);
			    					}

			    					printf("Datagram with sequence number %d was re-sent to client\n", send_buffer->base[send_buffer->front+j].seq);

								}
							}
						}

						more_to_send=sending_window-CirQueueLength(send_buffer);
						
						//server reads from file and sends new data to client
						for(j=0; j<more_to_send; j++) {

			    			localcopy.seq=++sequence_number;

			    			fread(localcopy.sendline, 1, packet_capacity, file);

			    			if(feof(file)==0) localcopy.ts=rtt_ts(&rttinfo);
			    			else localcopy.ts=0;

			    			EnCirQueue(send_buffer, localcopy);
			    
			    			send_hdr.seq=htonl(localcopy.seq);
			    			send_hdr.ts=htonl(localcopy.ts);

			    			if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

								perror("SERVER: Can't send data to client using send() function\n");
								exit(1);
			    			}

			    			if(send(childfd, localcopy.sendline, sizeof(localcopy.sendline), 0)<0) {

			    				perror("SERVER: Can't send data to client using send() function\n");
								exit(1);
			    			}

			    			printf("Server sent new datagram to client with sequence number: %d\n\n", sequence_number);

			    			bzero(localcopy.sendline, sizeof(localcopy.sendline));

			    			if(feof(file)) {

								printf("This is the end of the file\n\n");
								break;
			    			}
						}

						DataAlarmAgain:

						if(signal(SIGALRM, file_data_alrm)==SIG_ERR) {
            
        		    		perror("SERVER: Unable to catch SIGALRM signal\n");
        		    		exit(1);
    					}

						alarm(rtt_start(&rttinfo));

						//time out before receiving client ACKs
						if(sigsetjmp(jmpbuf_2, 1)!=0) {

			    			if(rtt_timeout(&rttinfo)<0) {

								perror("SERVER: No response from client, giving up\n");
								rttinit=0;
								errno=ETIMEDOUT;
								exit(1);
			    			}

			    			printf("Timeout before receiving ACKs from client\n");

							//if client receive window is full
			    			if(recv_window==0) {

			    				printf("Client receive buffer locks\n");

			    				FD_ZERO(&fdset);

			    				if(sigemptyset(&sigset_empty)<0 || sigemptyset(&sigset_alrm)<0) {

				    				perror("SERVER: Unable to empty signal set\n");
				    				exit(1);
								}

			    				if(sigaddset(&sigset_alrm, SIGALRM)<0) {

				    				perror("SERVER: Unable to put SIGALRM to alarm signal set\n");
				    				exit(1);
			    				}

			    				if(sigprocmask(SIG_BLOCK, &sigset_alrm, NULL)<0) {

				    				perror("SERVER: Unable to mask alarm signal set\n");
				   					exit(1);
			    				}

			    				if(signal(SIGALRM, buffer_alrm)==SIG_ERR) {
            
        			    			perror("SERVER: Unable to catch SIGALRM signal\n");
        			    			exit(1);
    			    			}

			    				while(1) {

				    				alarm(3);
				    				FD_SET(childfd, &fdset);

				    				printf("Server is waiting for client to annouce new advertised window size\n");
	
				    				if(pselect(childfd+1, &fdset, NULL, NULL, NULL, &sigset_empty)<0) {

				    					if(errno==EINTR) {

											send_hdr.seq=htonl(0);
			    							send_hdr.ts=htonl(0);

			    							if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

					    						perror("SERVER: Can't send data to client using send() function\n");
					    						exit(1);
			    							}
			    							printf("Server sent advertised window size update request to client\n");

											continue;
				    					}
				    					else {

											perror("SERVER: Function pselect() error\n");
											exit(1);
				    					}
									}

									if(recv(childfd, &recv_hdr, sizeof(recv_hdr), 0)<0) {

				    					perror("SERVER: Can't read data from client using recv() function\n");
				    					exit(1);
			        				}

			        				printf("Updated new advertised window size: %d\n", ntohs(recv_hdr.recv_window));

			    					if((recv_window=ntohs(recv_hdr.recv_window))>0) break;
			        			}
			    			}

							//congestion window shrinks to size one and first unacknowleged datagram is sent again
			    			ssh=cwnd/2>1?cwnd/2:1;
			    			cwnd=1;
			    			sending_window=cwnd;

			    			send_hdr.seq=htonl(send_buffer->base[send_buffer->front].seq);
			    			send_hdr.ts=htonl(send_buffer->base[send_buffer->front].ts);

			    			if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

								perror("SERVER: Can't send data to client using send() function\n");
								exit(1);
			    			}

			    			if(send(childfd, send_buffer->base[send_buffer->front].sendline, 
			    					sizeof(send_buffer->base[send_buffer->front].sendline), 0)<0) {

			    				perror("SERVER: Can't send data to client using send() function\n");
								exit(1);
			    			}

			    			printf("Datagram with sequence number %d was re-sent to client\n", send_buffer->base[send_buffer->front].seq);

			    			goto DataAlarmAgain;
						}

						//server waits for ACKs from client
						measure_rtt=0;
						received=0;			

						do {

			    			if(recv(childfd, &recv_hdr, sizeof(recv_hdr), 0)<0) {

								perror("SERVER: Can't read data from client using recv() function\n");
								exit(1);
			    			}

			    			if(cwnd<ssh) cwnd++;
			    			else {

								if(++congestion_avoid_count==cwnd) {

				    				cwnd++;
				    				congestion_avoid_count=0;
								}
			    			}

			    			received++;

			    			recv_hdr.seq=ntohl(recv_hdr.seq);

			    			printf("Received ACK from client: %d\n\n", recv_hdr.seq);

			    			if(measure_rtt==0) measure_rtt=rtt_ts(&rttinfo)-ntohl(recv_hdr.ts);
			    			if(recv_hdr.seq==0) goto End;

			    			if(recv_hdr.seq>head) {

			    				for( ; head!=recv_hdr.seq; head++) DeCirQueue(send_buffer);
			    			}

			    			recv_window=ntohs(recv_hdr.recv_window);
			    			printf("Advertised window size: %d\n\n", recv_window);

							//fast retransmit
			    			if(++(send_buffer->base[send_buffer->front].ack)==3) {

								alarm(0);
								send_buffer->base[send_buffer->front].ack=0;

								printf("Fast retransmit: three duplicate ACKs were received\n");

								if(rtt_timeout(&rttinfo)<0) {

				    				perror("SERVER: No response from client, giving up\n");
				    				rttinit=0;
				    				errno=ETIMEDOUT;
				    				exit(1);
			    				}

								//if client receive window is full
			    				if(recv_window==0) {

			    	    			printf("Client receive buffer locks\n");

			    	    			FD_ZERO(&fdset);

			    	    			if(sigemptyset(&sigset_empty)<0 || sigemptyset(&sigset_alrm)<0) {

				    					perror("SERVER: Unable to empty signal set\n");
				    					exit(1);
				    				}

			    	    			if(sigaddset(&sigset_alrm, SIGALRM)<0) {

				    					perror("SERVER: Unable to put SIGALRM to alarm signal set\n");
				    					exit(1);
			    	    			}

			    	    			if(sigprocmask(SIG_BLOCK, &sigset_alrm, NULL)<0) {

				    					perror("SERVER: Unable to mask alarm signal set\n");
				    					exit(1);
			    	    			}

			    	    			if(signal(SIGALRM, buffer_alrm)==SIG_ERR) {
            
        			    				perror("SERVER: Unable to catch SIGALRM signal\n");
        			    				exit(1);
    			    				}	

			    	    			while(1) {

				    					alarm(3);
				    					FD_SET(childfd, &fdset);

				    					printf("Server is waiting for client to annouce new advertised window size\n");
	
				    					if(pselect(childfd+1, &fdset, NULL, NULL, NULL, &sigset_empty)<0) {

				    	    				if(errno==EINTR) {

												send_hdr.seq=htonl(0);
			    								send_hdr.ts=htonl(0);

			    								if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

					    							perror("SERVER: Can't send data to client using send() function\n");
					    							exit(1);
			    								}
			    								printf("Server sent advertised window size update request to client\n");

												continue;
				    						}
				    						else {

												perror("SERVER: Function pselect() error\n");
												exit(1);
				    						}
										}

										if(recv(childfd, &recv_hdr, sizeof(recv_hdr), 0)<0) {

				    						perror("SERVER: Can't read data from client using recv() function\n");
				    						exit(1);
			        					}

			        					printf("Updated new advertised window size: %d\n", ntohs(recv_hdr.recv_window));

			    						if((recv_window=ntohs(recv_hdr.recv_window))>0) break;
			            			}
								}

								//fast recovery
			    				cwnd=cwnd/2>1?cwnd/2:1;  
			    				ssh=cwnd;

								send_hdr.seq=htonl(send_buffer->base[send_buffer->front].seq);
			    				send_hdr.ts=htonl(send_buffer->base[send_buffer->front].ts);

			    				if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

									perror("SERVER: Can't send data to client using send() function\n");
									exit(1);
			    				}

			    				if(send(childfd, send_buffer->base[send_buffer->front].sendline, 
			    						sizeof(send_buffer->base[send_buffer->front].sendline), 0)<0) {

			    					perror("SERVER: Can't send data to client using send() function\n");
									exit(1);
			    				}

			    				printf("Datagram of the duplicate ACKs was re-sent to client\n");

								if(!feof(file)) {

				    				sending_window=three_min(cwnd, send_window, recv_window)*2;

									//server reads from file and sends new data to client
									for(j=0; j<sending_window/2-1; j++) {

			    						localcopy.seq=++sequence_number;

			    						fread(localcopy.sendline, 1, packet_capacity, file);

			    						if(feof(file)==0) localcopy.ts=rtt_ts(&rttinfo);
			    						else localcopy.ts=0;

			    						EnCirQueue(send_buffer, localcopy);
			    
			    						send_hdr.seq=htonl(localcopy.seq);
			    						send_hdr.ts=htonl(localcopy.ts);

			    						if(send(childfd, &send_hdr, sizeof(send_hdr), 0)<0) {

											perror("SERVER: Can't send data to client using send() function\n");
											exit(1);
			    						}

			    						if(send(childfd, localcopy.sendline, sizeof(localcopy.sendline), 0)<0) {

			    							perror("SERVER: Can't send data to client using send() function\n");
											exit(1);
			    						}

			    						printf("Server sent new datagram to client with sequence number: %d\n", sequence_number);

			    						bzero(localcopy.sendline, sizeof(localcopy.sendline));

			    						if(feof(file)) {

											printf("This is the end of the file\n\n");
											break;
			    						}
									}
								}

								goto DataAlarmAgain;
							}

						} while(received!=sending_window && IsEmptyCirQueue(send_buffer)==0);

						alarm(0);

						rtt_stop(&rttinfo, measure_rtt);
		    		}
    			}
	    	}
		}
    }

    End:

    if(send(childfd, &i, sizeof(int), 0)<0) {

		perror("SERVER: Can't send data to client using send() function\n");
		exit(1);
	}

    printf("File data transmission completed and server child process is closing\n\n");

    if(close(childfd)<0) {

		perror("SERVER: Socket close error\n");
		exit(1);
	}

    return 0;
}

void sig_chld(int signo) {
    
    pid_t pid;
    int stat;

    pid=wait(&stat);
    printf("Child process %d terminated\n\n", pid);
    return;
}

void handshake_data_alrm(int signo) {

    siglongjmp(jmpbuf_1, 1);
}

void file_data_alrm(int signo) {

	siglongjmp(jmpbuf_2, 1);
}

void buffer_alrm(int signo) {

    return;
}

int three_min(int first, int second, int third) {

    first=first<second?first:second;
    first=first<third?first:third;
    return first;
}
