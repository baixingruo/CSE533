//
//  tour.c
//  HW4
//
//  Created by Panchen Xue on 12/1/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "unp.h"
#include "hw_addrs.h"

#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <time.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>
#include <string.h>

#define PROTO 0x40
#define ID 0x2404

struct hwaddr {

	int sll_ifindex;
	unsigned short sll_hatype;
	unsigned char sll_halen;
	unsigned char sll_addr[8];
};

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
	
char* multicast_ip="224.1.1.1";
char* multicast_port="50000";

char ping_addr[10][15];
int ping_cnt[10];
int ping_no=0;

int sendfd;
struct sockaddr *sasend;
socklen_t salen;

int node=0;

int visited=0;
int option=0;
int last_node=0;

int areq(struct sockaddr *, socklen_t, struct hwaddr *);
void sig_alrm(int);

int main(int argc, char **argv) {
	
	printf("\nWELCOME! THIS IS TOUR SERVICE\n");

	struct hwa_info *hwa;
    struct sockaddr *sa;
	int index;

	if((hwa=get_hw_addrs())==NULL) {

		perror("TOUR: Failed to get interface information for this node\n");
		exit(1);
	}

	sa=hwa->hwa_next->ip_addr;

	for(index=0; index<10; index++) {

		if(strcmp(Sock_ntop_host(sa, sizeof(*sa)), vm_addr[index])==0) {
		
			node=index+1;
			break;
		}
	}
	printf("You are on the VM%d node now\n\n", node);
	
	int routefd;
	int replyfd, requestfd;
	const int on=1;

	if((routefd=socket(AF_INET, SOCK_RAW, PROTO))<0) {
	
		perror("TOUR: Failed to open raw socket for source route\n");
		exit(1);
	}

	if(setsockopt(routefd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on))<0) {
	
		perror("TOUR: Failed to set IP_HDRINCL option for route raw socket\n");
		exit(1);
	}
	printf("Raw socket for source route is open and IP_HDRINCL option is set\n");
	
	if((replyfd=socket(AF_INET, SOCK_RAW, htons(IPPROTO_ICMP)))<0) {
	
		perror("TOUR: Failed to open raw socket for ICMP echo reply\n");
		exit(1);
	}
	printf("Raw socket for ICMP echo reply is open\n\n");
	
	if((requestfd=socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP)))<0) {
	
		perror("TOUR: Failed to open PF_PACKET socket for ICMP echo request\n");
		exit(1);
	}
	printf("PF_PACKET socket for ICMP echo request is open\n\n");
	
	int recvfd;
	struct sockaddr *sarecv;

 	if((sendfd=udp_client(multicast_ip, multicast_port, (SA **) &sasend, &salen))<0) {
 	
 		perror("TOUR: Failed to open UDP socket for sending multicasting\n");
 		exit(1);
 	}
 	printf("UDP socket for sending multicasting is open\n");
 	
 	if((recvfd=socket(sasend->sa_family, SOCK_DGRAM, 0))<0) {
 	
 		perror("TOUR: Failed to open UDP socket for receiving multicasting\n");
 		exit(1);
 	}
 	
 	if(setsockopt(recvfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0) {
 	
 		perror("TOUR: Failed to set SO_REUSEADDR option for receiving UDP socket\n");
 		exit(1);
 	}
 	
 	sarecv=(struct sockaddr *) malloc(salen);
 	memcpy(sarecv, sasend, salen);
 	
 	if(bind(recvfd, sarecv, salen)<0) {
 	
 		perror("TOUR: Failed to bind receiving UDP socket\n");
 		exit(1);
 	}
 	printf("UDP socket for receiving multicasting is open and binded\n\n");
 	
 	int offset, total;
	
	if(argc==1) {
        
        printf("This is not the source node for source routing\n\n");
    }
    else {
    
    	printf("This is the source node for source routing\n\n");
    	visited=1;
    	
    	if(mcast_join(recvfd, sasend, salen, NULL, 0)<0 || mcast_set_ttl(sendfd, 1)<0) {
 	
 			perror("TOUR: Failed to join multicasting group or disable the loopback feature or set TLL to one\n");
 			exit(1);
 		}
 		printf("UDP socket for receiving multicasting joined multicast group\n");
 		printf("UDP socket for sending multicasting set TLL to one\n\n");
	
		void *ip_packet=(void *) malloc(ETH_FRAME_LEN);
		struct ip *iphdr=(struct ip *) ip_packet;
		void *payload=(void *) malloc(1000);
		
		offset=2;
		total=argc;
		
		memcpy((void *) payload, (void *) multicast_ip, 10);
		memcpy((void *) (payload+10), (void *) &multicast_port, 2);
		memcpy((void *) (payload+12), (void *) &offset, 4);
		memcpy((void *) (payload+16), (void *) &total, 4);
		
		char source_route[20][15];
		int number;
		int datalen;
		
		strcpy(source_route[0], vm_addr[node-1]);
		printf("No.1 source route node is VM%d with IP address %s\n", node, vm_addr[node-1]);
		
		for(index=1; index<argc; index++) {
	
			if(strlen(argv[index])==3) {
		
				number=atoi(argv[index]+2);
				strcpy(source_route[index], vm_addr[number-1]);
				printf("No.%d source route node is VM%d with IP address %s\n", index+1, number, source_route[index]);
			}
			else {
		
				strcpy(source_route[index], vm_addr[9]);
				printf("No.%d source route node is VM10 with IP address 130.245.156.20\n", index+1);
			}
		}
			
		memcpy((void *) (payload+20), (void *) source_route, 15*argc);
		printf("\nData payload of source route packet was filled in\n");
		datalen=15*argc+20;
	
		iphdr->ip_hl=sizeof(struct ip)>>2;
		iphdr->ip_v=IPVERSION;
		iphdr->ip_tos=0;
		iphdr->ip_len=htons(sizeof(struct ip)+datalen);
		iphdr->ip_id=htons(ID);
		iphdr->ip_off=htons(0);
		iphdr->ip_ttl=1;
		iphdr->ip_p=PROTO;
		
		if(inet_pton(AF_INET, vm_addr[node-1], &(iphdr->ip_src))!=1 || inet_pton(AF_INET, source_route[1], &(iphdr->ip_dst))!=1) {
		
			perror("TOUR: Failed to set source or destination IP address in IP header\n");
			exit(1);
		}

  		printf("IP header of source route packet was filled in\n\n");
  		
  		memcpy((void *) (ip_packet+sizeof(struct ip)), (void *) payload, datalen);
	
		struct sockaddr_in *sin=(struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
		bzero(sin, sizeof (struct sockaddr_in));
		sin->sin_port=htons(60000);
  		sin->sin_family=AF_INET;
  		sin->sin_addr.s_addr=iphdr->ip_dst.s_addr;
  		
  		if(sendto(routefd, ip_packet, sizeof(struct ip)+datalen, 0, (struct sockaddr *) sin, sizeof(struct sockaddr))<0) {
  		
    		perror("TOUR: Failed to send out source route packet on raw socket\n");
    		exit(1);
  		}
  		
  		printf("Source route packet was sent out on source route raw socket\n\n");
  		
  		free(ip_packet);
  		free(payload);
  		free(sin);
  	}
	
	int maxfdp=0;
	fd_set rset;
	
	char sendline[MAXLINE];
	char recvline[MAXLINE+1];
	ssize_t cnt;
	
	Signal(SIGALRM, sig_alrm);
	srand(time(NULL));
	
	while(1) {
		
		FD_ZERO(&rset);
		FD_SET(routefd, &rset);
		FD_SET(replyfd, &rset);
		FD_SET(recvfd, &rset);
		
		maxfdp=max(routefd, replyfd);
		maxfdp=max(maxfdp, recvfd);
		maxfdp++;
			
		printf("TOUR process is listening on raw sockets, PF_PACKET socket, and UDP socket...\n\n");
		
		if(select(maxfdp, &rset, NULL, NULL, NULL)<0) {
		
			if(errno==EINTR) continue;
			else {

				perror("TOUR: Failed to select from sockets\n");
				exit(1);
			}
		}

		if(FD_ISSET(routefd, &rset)) {
		
			printf("New source route packet was received\n\n");
			
			void *ip_packet=(void *) malloc(ETH_FRAME_LEN);
			struct ip *iphdr=(struct ip *) ip_packet;
			
			if((cnt=recvfrom(routefd, ip_packet, ETH_FRAME_LEN, 0, NULL, NULL))<0) {

				perror("ODR: Failed to receive Ethernet frame througth interface socket\n");
				exit(1);
			}
			
			if(ntohs(iphdr->ip_id)!=ID) {
			
				printf("Received source route packet didn't check out, the packet was ignored\n");
			}
			else {
			
				time_t ticks;
				char time_buff[MAXLINE];
				char src_ip[15];
			
				ticks=time(NULL);       
        		snprintf(time_buff, sizeof(time_buff), "%.24s\r\n", ctime(&ticks));
        		
        		if(inet_ntop(AF_INET, &(iphdr->ip_src), src_ip, 15)==NULL) {
        		
        			perror("TOUR: Faile to convert source IP address to presentation format from IP header\n");
        			exit(1);
        		}
        		
        		for(index=0; index<10; index++) {

					if(strcmp(src_ip, vm_addr[index])==0) break;
				}
        		
				printf("<%s> Received source route packet is from VM%d with IP address %s\n\n", time_buff, index+1, src_ip);
				
				if(visited==0) {
				
					printf("This is the first time this node is visited on source route\n\n");
					
					visited=1;
				
					if(mcast_join(recvfd, sasend, salen, NULL, 0)<0 || mcast_set_ttl(sendfd, 1)<0) {
 	
 						perror("TOUR: Failed to join multicasting group or disable the loopback feature or set TLL to one\n");
 						exit(1);
 					}
 					printf("UDP socket for receiving multicasting joined multicast group\n");
 					printf("UDP socket for sending multicasting set TLL to one\n\n");
 				}
 				
 				void *payload=(void *) malloc(1000);
 				char dst_ip[15];
 				
 				memcpy((void *) payload, (void *) (ip_packet+sizeof(struct ip)), cnt-sizeof(struct ip));
 				memcpy((void *) &offset, (void *) (payload+12), 4);
 				memcpy((void *) &total, (void *) (payload+16), 4);
 				
 				if(offset==total) {
 				
 					printf("This is the last node on the source route tour\n\n");
 					last_node=1;
 				}
 				else {
 				
 					offset++;
 					memcpy((void *) (payload+12), (void *) &offset, 4);
 					printf("Data payload of source route packet was filled in\n");
 					
 					memcpy((void *) dst_ip, (void *) (payload+offset*15+5), 15);
 					
 					bzero(ip_packet, cnt);
 					iphdr->ip_hl=sizeof(struct ip)>>2;
					iphdr->ip_v=IPVERSION;
					iphdr->ip_tos=0;
					iphdr->ip_len=htons(cnt);
					iphdr->ip_id=htons(ID);
					iphdr->ip_off=htons(0);
					iphdr->ip_ttl=1;
					iphdr->ip_p=PROTO;
		
					if(inet_pton(AF_INET, vm_addr[node-1], &(iphdr->ip_src))!=1 
							|| inet_pton(AF_INET, dst_ip, &(iphdr->ip_dst))!=1) {
		
						perror("TOUR: Failed to set source or destination IP address in IP header\n");
						exit(1);
					}
					
  					printf("IP header of source route packet was filled in\n\n");
  					
  					memcpy((void *) (ip_packet+sizeof(struct ip)), (void *) payload, cnt-sizeof(struct ip));
  				
  					struct sockaddr_in *sin=(struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
					bzero(sin, sizeof (struct sockaddr_in));
  					sin->sin_family=AF_INET;
  					sin->sin_addr.s_addr=iphdr->ip_dst.s_addr;
  		
  					if(sendto(routefd, ip_packet, cnt, 0, (struct sockaddr *) sin, 
  							sizeof(struct sockaddr))<0) {
  		
    					perror("TOUR: Failed to send out source route packet on raw socket\n");
    					exit(1);
  					}
  		
  					printf("Source route packet was sent out on source route raw socket\n\n");

  					free(sin);
  				}
  				
  				free(payload);
  				
  				struct sockaddr_in *IP=(struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
  				struct hwaddr *HW=(struct hwaddr *) malloc(sizeof(struct hwaddr));
  				
				IP->sin_family=AF_INET;
				if(inet_pton(AF_INET, src_ip, &(IP->sin_addr))!=1) {
				
					perror("TOUR: Failed to set destination IP address for local ARP\n");
					exit(1);
				}
				
				HW->sll_ifindex=2;
				HW->sll_hatype=ARPHRD_ETHER;
				HW->sll_halen=ETH_ALEN;
  				
  				printf("TOUR called API to sent ARP request\n\n");
  				
  				areq((struct sockaddr *) IP, sizeof(struct sockaddr), HW);
  				
  				printf("TOUR received ARP reply of IP address %s through API\n", src_ip);
  				printf("HW address ");
  				
  				unsigned char *ptr=HW->sll_addr;
  				for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n\n":":"); }
				  				  				  				  								
  				for(index=0; index<ping_no; index++) {
  				
  					if(strcmp(ping_addr[index], src_ip)==0) {
  					
  						printf("TOUR is already pinging this previous node\n\n");
  						break;
  					}
  				}
  				
  				if(index==ping_no) {
  				
  					strcpy(ping_addr[ping_no], src_ip);
  					ping_cnt[ping_no++]=0;
  					printf("TOUR is ready to ping previous node on source routing\n\n");
  					sig_alrm(SIGALRM);
  				}	
			}
			
			free(ip_packet);		
		}
		
		if(FD_ISSET(replyfd, &rset)) {	
	
		}
		
		if(FD_ISSET(recvfd, &rset)) {
		
			printf("New multicasting packet was received\n\n");
		
			struct sockaddr *safrom;
			safrom=malloc(salen);
			
			bzero(recvline, MAXLINE+1);
			
			cnt=Recvfrom(recvfd, recvline, MAXLINE, 0, safrom, &salen);
			recvline[cnt]=0;
			
			printf("Node VM%d received message %s\n\n", node, recvline);
			
			if(option==0) {
			
				option=1;
				printf("PING process has stopped\n\n");
			
				bzero(sendline, MAXLINE);
				cnt=snprintf(sendline, sizeof(sendline), "<<<<<Node VM%d: I am a member of the group>>>>>", node);

				if(sendto(sendfd, sendline, cnt, 0, sasend, salen)<0) {
					
					perror("TOUR: Failed to send multicasting\n");
					exit(1);
				}
				
 				printf("Node VM%d sent message %s\n\n", node, sendline);
 				
				alarm(5);
			}
		}
	}	
}

int areq(struct sockaddr *IP, socklen_t sockaddrlen, struct hwaddr *HW) {
	
	int unixfd;
	struct sockaddr_un srvaddr;

	if((unixfd=socket(AF_LOCAL, SOCK_STREAM, 0))<0) {

		perror("SERVER: Failed to open socket\n");
		exit(1);
	}
	
	srvaddr.sun_family=AF_LOCAL;
    strcpy(srvaddr.sun_path, "114042");

	if(connect(unixfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {
	
		perror("TOUR: Failed to connect to ARP well-known address\n");
		exit(1);
	}
	printf("API UNIX domain socket was open and connected\n\n");
	
	char ip_addr[15];
	int cnt;
	
	strcpy(ip_addr, inet_ntoa(((struct sockaddr_in *) IP)->sin_addr));
	
    if(writen(unixfd, ip_addr, 15)<0) {

		perror("TOUR: Failed to send data sequence to local ARP\n");
		exit(1);
	}	
	printf("API sent ARP request of IP address %s to local ARP\n\n", ip_addr);
	
	unsigned char mac_addr[6];
	
	if((cnt=read(unixfd, mac_addr, 6))<0) {

		perror("TOUR: Failed to receive data sequence from local ARP\n");
		exit(1);
	}
	printf("API received ARP reply from local ARP\n\n");
	
	memcpy(HW->sll_addr, mac_addr, 6);
	
	return 1;
}

void sig_alrm(int signo) {

	int index;
	
	if(option==0) {
		
		if(ping_cnt[0]==5 && last_node==1) {
		
			char sendline[MAXLINE];
			int cnt;
					
			cnt=snprintf(sendline, sizeof(sendline), 
					"<<<<<This is node VM%d. Tour has ended. Group members please identify yourselves>>>>>", node);

			if(sendto(sendfd, sendline, cnt, 0, sasend, salen)<0) {
					
				perror("TOUR: Failed to send multicasting\n");
				exit(1);
			}
 			printf("Node VM%d sent message %s\n\n", node, sendline);
 		}
 		else {
 		
 			for(index=0; index<ping_no; index++) {
 				
 				printf("PING %s: 56 data bytes\n", ping_addr[index]);
				printf("64 bytes from %s: seq=%d, ttl=53\n\n", 
						ping_addr[index], ping_cnt[index]++);								
			}
			
			alarm(1);
		}
		
		return;
	}
	else {
	
		printf("The TOUR process has all finished. GOODBYE!\n\n");
		exit(0);
	}
	
	return;	
}

