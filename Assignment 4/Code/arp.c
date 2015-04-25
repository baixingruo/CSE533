//
//  arp.c
//  HW4
//
//  Created by Panchen Xue on 12/3/14.
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

#define PROTO 0x2404

struct hdr {

	unsigned char dst_mac[ETH_ALEN];
	unsigned char src_mac[ETH_ALEN];
	unsigned short proto;
};

struct arp {

	uint16_t hard_type;
	uint16_t prot_type;
	uint8_t hard_size;
	uint8_t prot_size;
	uint16_t op;
	unsigned char src_mac[6];
	char src_ip[15];
	unsigned char dst_mac[6];
	char dst_ip[15];
};

struct cache_entry {

	char ip_addr[15];
	unsigned char mac_addr[6];
	int sockfd;
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
	
int main(int argc, char **argv) {
	
	printf("\nWELCOME! THIS IS ARP SERVICE\n");

	struct hwa_info *hwa;
    struct sockaddr *sa;
    char ip_addr[15];
    unsigned char mac_addr[6];
    unsigned char *ptr;
    int node;
	int index;

	if((hwa=get_hw_addrs())==NULL) {

		perror("ARP: Failed to get interface information for this node\n");
		exit(1);
	}

	sa=hwa->hwa_next->ip_addr;

	for(index=0; index<10; index++) {

		if(strcmp(Sock_ntop_host(sa, sizeof(*sa)), vm_addr[index])==0) {
		
			node=index+1;
			strcpy(ip_addr, vm_addr[index]);
			break;
		}
	}
	printf("You are on the VM%d node now\n\n", node);
	
	printf("Node interface eth0 IP address %s\n", ip_addr);
	printf("Node interface eth0 HW address ");
	
	memcpy(mac_addr, hwa->hwa_next->if_haddr, 6);
	ptr=mac_addr;
	
	index=IF_HADDR;
	do {
			
		printf("%.2x%s", *ptr++ & 0xff, (index==1)?"\n\n":":");
		
	} while(--index>0);
	
	int packetfd, unixfd;
	struct sockaddr_un srvaddr;
	
	if((packetfd=socket(PF_PACKET, SOCK_RAW, htons(PROTO)))<0) {
	
		perror("ARP: Failed to open PF_PACKET socket for ICMP echo request\n");
		exit(1);
	}
	printf("PF_PACKET socket for ICMP echo request and reply is open\n\n");
	
	if((unixfd=socket(AF_LOCAL, SOCK_STREAM, 0))<0) {

		perror("ARP: Failed to open socket\n");
		exit(1);
	}

	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sun_family=AF_LOCAL;
	strcpy(srvaddr.sun_path, "114042");

    unlink(srvaddr.sun_path);

	if(bind(unixfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {

		perror("ARP: Failed to bind socket\n");
		exit(1);
	}
	
	if(listen(unixfd, LISTENQ)<0) {
	
		perror("ARP: Failed to turn UNIX domain socket into listening socket\n");
		exit(1);
	}
	printf("UNIX domain socket was open and turned into listening socket\n\n");
	
	struct cache_entry cache[10];
	bzero(cache, 10*sizeof(struct cache_entry));
	printf("ARP local caches was initialized\n\n");
	
	int maxfdp;
	fd_set rset;
	
	while(1) {
		
		FD_ZERO(&rset);
		FD_SET(packetfd, &rset);
		FD_SET(unixfd, &rset);
		
		maxfdp=max(packetfd, unixfd);
		maxfdp++;
			
		printf("ARP process is listening on PF_PACKET socket, and UNIX socket...\n\n");
		
		if(select(maxfdp, &rset, NULL, NULL, NULL)<0) {

			perror("ARP: Failed to select from PF_PACKET and UNIX sockets\n");
			exit(1);
		}
		
		if(FD_ISSET(packetfd, &rset)) {
		
			printf("New ARP packet is received from PF_PACKET socket\n\n");
		
			void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
			struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));
			struct arp *arp=(struct arp *) malloc(sizeof(struct arp));

			if(recvfrom(packetfd, ether_frame, ETH_FRAME_LEN, 0, NULL, NULL)<0) {

				perror("ARP: Failed to receive Ethernet frame througth PF_PACKET socket\n");
				exit(1);
			}
			
			memcpy(header, (void *) ether_frame, sizeof(struct hdr));
			memcpy(arp, (void *) (ether_frame+sizeof(struct hdr)), sizeof(struct arp));
			
			printf("Ethernet header source HW address ");
			ptr=header->src_mac;
			for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
			printf("Ethernet header destination HW address");
			ptr=header->dst_mac;
			for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
			printf("Ethernet header frame type %d\n", ntohs(header->proto));
			printf("ARP message identification number 40\n");
			printf("ARP message type of hardware address 1\n");
			printf("ARP message type of protocol address 0x0800\n");
			printf("ARP message hardware address size 6\n");
			printf("ARP message protocol address size 4\n");
			printf("ARP message operation number %d\n", ntohs(arp->op));
			printf("ARP message source IP address %s\n", arp->src_ip);
			printf("ARP message source HW address ");
			ptr=arp->src_mac;
			for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
			printf("ARP message destination IP address %s\n", arp->dst_ip);
			printf("ARP message destination HW address ");
			ptr=arp->dst_mac;
			for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n\n":":"); }
			
			if(ntohs(arp->op)==1) {
			
				printf("Received packet is a ARP request\n\n");
				
				if(strcmp(arp->dst_ip, ip_addr)==0) {
				
					printf("ARP request is for this node\n\n");
					
					memcpy(header->dst_mac, header->src_mac, 6);
					memcpy(header->src_mac, mac_addr, 6);
					
					arp->op=htons(2);
					memcpy(arp->dst_mac, mac_addr, 6);
					
					struct sockaddr_ll send_if_addr;
					
					bzero(&send_if_addr, sizeof(struct sockaddr_ll));
					send_if_addr.sll_family=PF_PACKET;
					send_if_addr.sll_protocol=htons(PROTO);
					send_if_addr.sll_hatype=ARPHRD_ETHER;
					send_if_addr.sll_pkttype=PACKET_BROADCAST;
					send_if_addr.sll_halen=ETH_ALEN;
					send_if_addr.sll_ifindex=2;
					memcpy(send_if_addr.sll_addr, header->dst_mac, 6);
					send_if_addr.sll_addr[6]=0x00;
					send_if_addr.sll_addr[7]=0x00;

					bzero(ether_frame, ETH_FRAME_LEN);
					memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  					memcpy((void *) (ether_frame+sizeof(struct hdr)), (void *) arp, sizeof(*arp));
	
					if(sendto(packetfd, ether_frame, sizeof(*header)+sizeof(*arp), 0, 
							(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

						perror("ARP: Failed to send Ethernet frame through PF_PACKET socket\n");
						exit(1);
					}	
					printf("ARP reply was sent out throught PF_PACKET socket\n\n");
					
					printf("Ethernet header source HW address ");
					ptr=header->src_mac;
					for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
					printf("Ethernet header destination HW address");
					ptr=header->dst_mac;
					for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
					printf("Ethernet header frame type %d\n", ntohs(header->proto));
					printf("ARP message identification number 40\n");
					printf("ARP message type of hardware address 1\n");
					printf("ARP message type of protocol address 0x0800\n");
					printf("ARP message hardware address size 6\n");
					printf("ARP message protocol address size 4\n");
					printf("ARP message operation number %d\n", ntohs(arp->op));
					printf("ARP message source IP address %s\n", arp->src_ip);
					printf("ARP message source HW address ");
					ptr=arp->src_mac;
					for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
					printf("ARP message destination IP address %s\n", arp->dst_ip);
					printf("ARP message destination HW address ");
					ptr=arp->dst_mac;
					for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n\n":":"); }	
				}
				else {
				
					printf("ARP request is not for this node\n\n");
					printf("ARP local cache was updated accordingly\n\n");
				}
			}
			else {

				printf("Received packet is a ARP reply\n\n");
				
				for(index=0; index<10; index++) {
				
					if(strcmp(arp->dst_ip, vm_addr[index])==0) break;
				}
				
				strcpy(cache[index].ip_addr, arp->dst_ip);
				memcpy(cache[index].mac_addr, arp->dst_mac, 6);
				
				if(write(cache[index].sockfd, cache[index].mac_addr, 6)<0) {

					perror("ARP: Failed to send data sequence to local TOUR\n");
					exit(1);
				}	
				printf("ARP reply of IP address %s was sent out to local TOUR\n\n", cache[index].ip_addr);
				
				close(cache[index].sockfd);
			}
		
			free(header);
			free(arp);
			free(ether_frame);	
		}
		
		if(FD_ISSET(unixfd, &rset)) {
		
			printf("New ARP request is received from UNIX domain socket\n");
		
			struct sockaddr_un cliaddr;
			int connfd;
			socklen_t len=sizeof(cliaddr);
			char ip[15];
			
			if((connfd=accept(unixfd, (SA *) &cliaddr, &len))<0) {
			
				perror("ARP: Failed to accept on UNIX domain socket\n");
				exit(1);
			}
			
			if(read(connfd, ip, 15)<0) {

				perror("ARP: Failed to receive data sequence from local TOUR\n");
				exit(1);
			}

			printf("Received ARP request is for IP address %s\n\n", ip);
			
			for(index=0; index<10; index++) {

				if(strcmp(ip, vm_addr[index])==0) break;
			}
			
			if(strcmp(cache[index].ip_addr, vm_addr[index])==0) {
			
				printf("The ARP information is available in local cache\n\n");
    			
    			if(write(connfd, cache[index].mac_addr, 6)<0) {

					perror("ARP: Failed to send data sequence to local TOUR\n");
					exit(1);
				}	
				printf("ARP reply of IP address %s was sent out to local TOUR\n\n", ip_addr);
				
				close(connfd);
			}
			else {
			
				cache[index].sockfd=connfd;
			
				printf("The ARP information is not available in local cache\n\n");
			
				struct sockaddr_ll send_if_addr;
				struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));				
				struct arp *arp=(struct arp *) malloc(sizeof(struct arp));
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
				
				header->dst_mac[0]=0xff;		
				header->dst_mac[1]=0xff;
				header->dst_mac[2]=0xff;
				header->dst_mac[3]=0xff;
				header->dst_mac[4]=0xff;
				header->dst_mac[5]=0xff;
				memcpy(header->src_mac, mac_addr, 6);
				header->proto=htons(PROTO);
				
				arp->hard_type=htons(1);
				arp->prot_type=htons(0x0800);
				arp->hard_size=6;
				arp->prot_size=4;
				arp->op=htons(1);
				memcpy(arp->src_mac, mac_addr, 6);
				strcpy(arp->src_ip, ip_addr);
				strcpy(arp->dst_ip, ip);
				arp->dst_mac[0]=0xff;
				arp->dst_mac[1]=0xff;
				arp->dst_mac[2]=0xff;
				arp->dst_mac[3]=0xff;
				arp->dst_mac[4]=0xff;
				arp->dst_mac[5]=0xff;
				
				bzero(&send_if_addr, sizeof(struct sockaddr_ll));
				send_if_addr.sll_family=PF_PACKET;
				send_if_addr.sll_protocol=htons(PROTO);
				send_if_addr.sll_hatype=ARPHRD_ETHER;
				send_if_addr.sll_pkttype=PACKET_BROADCAST;
				send_if_addr.sll_halen=ETH_ALEN;
				send_if_addr.sll_ifindex=2;
				send_if_addr.sll_addr[0]=0xff;		
				send_if_addr.sll_addr[1]=0xff;		
				send_if_addr.sll_addr[2]=0xff;
				send_if_addr.sll_addr[3]=0xff;
				send_if_addr.sll_addr[4]=0xff;
				send_if_addr.sll_addr[5]=0xff;
				send_if_addr.sll_addr[6]=0x00;
				send_if_addr.sll_addr[7]=0x00;

				memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  				memcpy((void *) (ether_frame+sizeof(struct hdr)), (void *) arp, sizeof(*arp));

				if(sendto(packetfd, ether_frame, sizeof(*header)+sizeof(*arp), 0, 
						(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

					perror("ARP: Failed to send Ethernet frame through PF_PACKET socket\n");
					exit(1);
				}
				printf("ARP request was broadcast throught PF_PACKET socket\n\n");
				
				printf("Ethernet header source HW address ");
				ptr=header->src_mac;
				for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
				printf("Ethernet header destination HW address");
				ptr=header->dst_mac;
				for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
				printf("Ethernet header frame type %d\n", ntohs(header->proto));
				printf("ARP message identification number 40\n");
				printf("ARP message type of hardware address 1\n");
				printf("ARP message type of protocol address 0x0800\n");
				printf("ARP message hardware address size 6\n");
				printf("ARP message protocol address size 4\n");
				printf("ARP message operation number %d\n", ntohs(arp->op));
				printf("ARP message source IP address %s\n", arp->src_ip);
				printf("ARP message source HW address ");
				ptr=arp->src_mac;
				for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n":":"); }
				printf("ARP message destination IP address %s\n", arp->dst_ip);
				printf("ARP message destination HW address ");
				ptr=arp->dst_mac;
				for(index=0; index<6; index++) { printf("%.2x%s", *ptr++ & 0xff, (index==5)?"\n\n":":"); }

				free(header);
				free(arp);
				free(ether_frame);			
			}		
		}
	}
}

