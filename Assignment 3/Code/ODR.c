//
//  ORD.c
//  HW3
//
//  Created by Panchen Xue on 11/12/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include "hw_addrs.h"
#include "API.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <time.h>

#define PROTO 0x4042

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

int broadcast_id=0;
//port number assigned to each local clients
int port=6000;

//local routing table
struct routing_entry {

	char dst_addr[15];	
	unsigned char next_hop[ETH_ALEN];	
	int outgoing_if;
	int hop_cnt;
	int broadcast_id;
};

//local UNIX domain socket address
struct port_entry {

	int port;
	char sun_path[15];
};

//Ethernet frame header
struct hdr {

	unsigned char dst_mac[ETH_ALEN];
	unsigned char src_mac[ETH_ALEN];
	unsigned short proto;
	int type;
};

//routing request
struct RREQ {

	char src_addr[15];
	char dst_addr[15];
	int src_port;
	int broadcast_id;
	int hop_cnt;
	int RREP_sent;
	int discovery;
};

//routing reply
struct RREP {

	char src_addr[15];
	char dst_addr[15];
	int dst_port;
	int hop_cnt;
	int discovery;
};

//application payload
struct payload {

	char src_addr[15];
	char dst_addr[15];
	int src_port;
	int dst_port;
	int hop_cnt;
	int len;
	char message[25];
};

//print out node interfaces infomation
void prhwaddrs(struct hwa_info *hwa) {

	struct sockaddr *sa;
    char *ptr;
    int index, prflag;

    printf("\n");

    for( ; hwa!=NULL; hwa=hwa->hwa_next) {

        printf("%s :%s", hwa->if_name, ((hwa->ip_alias)==IP_ALIAS)?" (alias)\n":"\n");

        if((sa=hwa->ip_addr)!=NULL) printf("    IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));

        prflag=0;
        index=0;
        do {
            
			if(hwa->if_haddr[index]!='\0') {

            	prflag=1;
                break;
            }
        } while(++index<IF_HADDR);

        if(prflag) {

            printf("    HW addr = ");
           	ptr=hwa->if_haddr;
            index=IF_HADDR;

			do {
			
            	printf("%.2x%s", *ptr++ & 0xff, (index==1)?" ":":");
            } while(--index>0);
        }

        printf("\n    interface index = %d\n\n", hwa->if_index);
    }
}

int main(int argc, char **argv) {

	if(argc!=2) {
        
       	perror("ODR: Usage is 'ODR staleness_time(in seconds)'\n");
        exit(1);
    }
    
    printf("\nWELCOME! THIS IS ODR SERVICE\n\n");

	struct hwa_info *hwahead, *hwa;
    struct sockaddr *sa;
    //local interfaces information
	struct sockaddr_ll if_addr[3];
	int if_fd[3];
	int if_cnt=0;
	unsigned char *ptr;
	int node;
	int cnt;
	//index helper
	int index, number, count, no;

	if((hwahead=get_hw_addrs())==NULL) {

		perror("ODR: Failed to get interface information for this node\n");
		exit(1);
	}

	//get node interfaces information
	for(hwa=hwahead; hwa!=NULL; hwa=hwa->hwa_next) {

		//loopback address
		if(hwa->if_index==1) continue;
		//get node index
		else if(hwa->if_index==2) {

			sa=hwa->ip_addr;

			for(index=0; index<10; index++) {

				if(strcmp(Sock_ntop_host(sa, sizeof(*sa)), vm_addr[index])==0) {
		
					node=index+1;
					break;
				}
			}

			continue;
		}

		//save node interface information
		if_addr[if_cnt].sll_family=PF_PACKET;
		if_addr[if_cnt].sll_protocol=htons(PROTO);
		if_addr[if_cnt].sll_ifindex=hwa->if_index;
		memcpy(if_addr[if_cnt].sll_addr, hwa->if_haddr, ETH_ALEN);

		//create a PF_PACKET socket for every interface of interest
		if((if_fd[if_cnt]=socket(PF_PACKET, SOCK_RAW, htons(PROTO)))<0) {

			perror("ODR: Failed to open interface raw socket\n");
			exit(1);
		}

		if(bind(if_fd[if_cnt], (SA *) &if_addr[if_cnt], sizeof(if_addr[if_cnt]))<0) {

			perror("ODR: Failed to bind interface raw socket\n");
			exit(1);
		}

		if_cnt++;
	}	
	
	printf("You are on the VM%d node now\n", node);
	printf("Node interface list:");
	prhwaddrs(hwahead);
	printf("PF_PACKET sockets were created and binded for %d interface(s) of interest\n", if_cnt);

	//UNIX domain datagram socket for communication with application processes
	int odrfd;
	//UNIX domain socket address for ODR, client, and server 
    struct sockaddr_un odraddr, localaddr;
	socklen_t un_len=sizeof(struct sockaddr_un);

	bzero(&odraddr, sizeof(struct sockaddr_un));
	bzero(&localaddr, sizeof(struct sockaddr_un));
	
	odraddr.sun_family=AF_LOCAL;
	localaddr.sun_family=AF_LOCAL;	

	if((odrfd=socket(AF_LOCAL, SOCK_DGRAM, 0))<0) {

		perror("ODR: Failed to open UNIX domain socket\n");
		exit(1);
	}

	//ODR well-known sun_path	
	unlink("114042");	
	strcpy(odraddr.sun_path, "114042");
	
	if(bind(odrfd, (SA *) &odraddr, sizeof(odraddr))<0) {

		perror("ODR: Failed to bind UNIX domain socket\n");
		exit(1);
	}
	printf("ODR UNIX domain socket was created and binded\n");
	
	//local routing table
	struct routing_entry routing_table[10];
	//local port talbe
	struct port_entry port_table[10];
	int port_cnt=0;

	//initialize each destination node address in routing table
	for(index=0 ; index<10; index++) {
	
		strcpy(routing_table[index].dst_addr, vm_addr[index]);
		routing_table[index].outgoing_if=0;
	}
	printf("ODR local routing table was initialized\n");

	//put server well-known port number in port table	
	port_table[0].port=5000;
	strcpy(port_table[0].sun_path, "240411");

	int maxfdp;
	fd_set rset;
	
	char dst_addr[15];
	int dst_port;
	char message[MAXLINE];
	int flag;
	
	char recvmsg[MAXLINE+1];
	int route_exist;
	
	struct sockaddr_ll send_if_addr;
	unsigned char recv_mac[ETH_ALEN];
							
	while(1) {
		
		FD_ZERO(&rset);
		FD_SET(odrfd, &rset);
		maxfdp=odrfd;

		for(index=0; index<if_cnt; index++) {

			FD_SET(if_fd[index], &rset);
			maxfdp=max(if_fd[index], maxfdp);
		}
		maxfdp++;

		//listen on UNIX domain socket and all interface PF_PACKET sockets
		printf("\nODR is listen on UNIX domain socket and all interface PF_PACKET sockets\n\n");
			
		if(select(maxfdp, &rset, NULL, NULL, NULL)<0) {

			perror("ODR: Failed to select from unix domain and interface sockets\n");
			exit(1);
		}

		//request from local client or reply from local server	
		if(FD_ISSET(odrfd, &rset)) {

			if(recvfrom(odrfd, recvmsg, MAXLINE, 0, (SA *) &localaddr, &un_len)<0) {

				perror("ODR: Failed to receive data sequence from UNIX domain socket\n");
				exit(1);
			}
			printf("New message was received from local with sun_path: %s\n", localaddr.sun_path);

			sscanf(recvmsg, "%s %d %s %d", dst_addr, &dst_port, message, &flag);
				
			//check whether client port number already in port table
			for(index=0; index<=port_cnt; index++) {

				//client port number already in port table				
				if(strcmp(port_table[index].sun_path, localaddr.sun_path)==0) break;
			}
			
			//reply from local server
			if(index==0) {
			
				printf("Received message is local server reply\n\n");
				
				//find corresponding routing entry
				for(number=0; number<10; number++) {

					if(strcmp(routing_table[number].dst_addr, dst_addr)==0) break;
				}

				struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));				
				struct payload *load=(struct payload *) malloc(sizeof(struct payload));
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
				//application payload header
				memcpy(header->dst_mac, routing_table[number].next_hop, ETH_ALEN);
				memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
				header->proto=htons(PROTO);
				header->type=htonl(2);

				//application payload ODR protocol message
				strcpy(load->src_addr, vm_addr[node-1]);
				strcpy(load->dst_addr, dst_addr);
				load->src_port=htonl(5000);
				load->dst_port=htonl(dst_port);
				load->hop_cnt=htonl(0);
				load->len=htonl(sizeof(message));
				strcpy(load->message, message);
				
				//application payload Ethernet frame
				memcpy((void *) ether_frame, (void *) header, sizeof(*header));
				memcpy((void *) (ether_frame+sizeof(*header)), (void *) load, sizeof(*load));
				printf("Application payload is ready to be sent out to ");
				ptr=header->dst_mac;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);

				bzero(&send_if_addr, sizeof(struct sockaddr_ll));
				send_if_addr.sll_family=PF_PACKET;
				send_if_addr.sll_protocol=htons(PROTO);
				send_if_addr.sll_ifindex=routing_table[number].outgoing_if;
				send_if_addr.sll_halen=ETH_ALEN;
				memcpy(send_if_addr.sll_addr, routing_table[number].next_hop, ETH_ALEN);
				send_if_addr.sll_addr[6]=0x00;
				send_if_addr.sll_addr[7]=0x00;

				//application payload is sent
				if(sendto(if_fd[routing_table[number].outgoing_if-3], ether_frame, sizeof(*header)+sizeof(*load), 0, 
						(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

					perror("ODR: Failed to send Ethernet frame throught interface socket\n");
					exit(1);
				}

				printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
				ptr=send_if_addr.sll_addr;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);
				
				printf("	ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);

				free(header);
				free(load);
				free(ether_frame);
																										
			}
			//request from local client
			else {
			
				printf("Received message is local client request\n\n");
			
				//client port number not in port talbe			
				if(index==port_cnt+1) {

					port_table[++port_cnt].port=port++;
					strcpy(port_table[port_cnt].sun_path, localaddr.sun_path);
				}
	
				//check whether route discovery is needed		
				route_exist=0;
				for(number=0; number<10; number++) {

					if(strcmp(routing_table[number].dst_addr, dst_addr)==0) {

						if(routing_table[number].outgoing_if!=0 && flag==0) route_exist=1;
						break;
					}
				}
			
				//route discovery is not needed
				//application payload is sent
				if(route_exist==1) {
			
					printf("Routing information is available for client request\n");

					struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));				
					struct payload *load=(struct payload *) malloc(sizeof(struct payload));
					void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
					//application payload header
					memcpy(header->dst_mac, routing_table[number].next_hop, ETH_ALEN);
					memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
					header->proto=htons(PROTO);
					header->type=htonl(2);

					//application payload ODR protocol message
					strcpy(load->src_addr, vm_addr[node-1]);
					strcpy(load->dst_addr, dst_addr);
					load->src_port=htonl(port_table[index].port);
					load->dst_port=htonl(5000);
					load->hop_cnt=htonl(0);
					load->len=htonl(sizeof(message));
					strcpy(load->message, message);
				
					//application payload Ethernet frame
					memcpy((void *) ether_frame, (void *) header, sizeof(*header));
					memcpy((void *) (ether_frame+sizeof(*header)), (void *) load, sizeof(*load));
					printf("Application payload is ready to be sent out to ");
					ptr=header->dst_mac;
        			no=ETH_ALEN;
					do {
			
            			printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        			} while(--no>0);

					bzero(&send_if_addr, sizeof(struct sockaddr_ll));
					send_if_addr.sll_family=PF_PACKET;
					send_if_addr.sll_protocol=htons(PROTO);
					send_if_addr.sll_ifindex=routing_table[number].outgoing_if;
					send_if_addr.sll_halen=ETH_ALEN;
					memcpy(send_if_addr.sll_addr, routing_table[number].next_hop, ETH_ALEN);
					send_if_addr.sll_addr[6]=0x00;
					send_if_addr.sll_addr[7]=0x00;

					//application payload is sent
					if(sendto(if_fd[routing_table[number].outgoing_if-3], ether_frame, 
							sizeof(*header)+sizeof(*load), 0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {

						perror("ODR: Failed to send Ethernet frame throught interface socket\n");
						exit(1);
					}

					printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
					ptr=send_if_addr.sll_addr;
        			no=ETH_ALEN;
					do {
			
            			printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        			} while(--no>0);
					
					printf("	ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);

					free(header);
					free(load);
					free(ether_frame);
				}
				//route discovery is needed
				//RREQ is sent
				else {
			
					printf("Routing information is not available for client request\n");

					struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));				
					struct RREQ *RRQ=(struct RREQ *) malloc(sizeof(struct RREQ));
					void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
				
					//RREQ header
					header->dst_mac[0]=0xff;		
					header->dst_mac[1]=0xff;
					header->dst_mac[2]=0xff;
					header->dst_mac[3]=0xff;
					header->dst_mac[4]=0xff;
					header->dst_mac[5]=0xff;
					header->proto=htons(PROTO);
					header->type=htonl(0);

					//RREQ ODR protocol message
					strcpy(RRQ->src_addr, vm_addr[node-1]);
					strcpy(RRQ->dst_addr, dst_addr);
					RRQ->src_port=htonl(port_table[index].port);
					RRQ->broadcast_id=htonl(++broadcast_id);
					RRQ->hop_cnt=htonl(0);
					RRQ->RREP_sent=htonl(0);
					RRQ->discovery=htonl(flag==1?1:0);
					printf("RREQ is ready to be sent out to ff:ff:ff:ff:ff:ff\n");

					bzero(&send_if_addr, sizeof(struct sockaddr_ll));
					send_if_addr.sll_family=PF_PACKET;
					send_if_addr.sll_protocol=htons(PROTO);
					send_if_addr.sll_halen=ETH_ALEN;
					send_if_addr.sll_addr[0]=0xff;		
					send_if_addr.sll_addr[1]=0xff;		
					send_if_addr.sll_addr[2]=0xff;
					send_if_addr.sll_addr[3]=0xff;
					send_if_addr.sll_addr[4]=0xff;
					send_if_addr.sll_addr[5]=0xff;
					send_if_addr.sll_addr[6]=0x00;
					send_if_addr.sll_addr[7]=0x00;

					//RREQ floods on all interface socket
					for(number=0; number<if_cnt; number++) {

						memcpy(header->src_mac, if_addr[number].sll_addr, ETH_ALEN);
					
						memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  						memcpy((void *) (ether_frame+sizeof(struct hdr)), (void *) RRQ, sizeof(*RRQ));
  					
  						send_if_addr.sll_ifindex=number+3;

						if(sendto(if_fd[number], ether_frame, sizeof(*header)+sizeof(*RRQ), 0, 
								(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

							perror("ODR: Failed to send Ethernet frame through interface socket\n");
							exit(1);
						}
						printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
									node, node);
						printf("	ODR message type 0, source ip %s, destination ip %s\n", RRQ->src_addr, RRQ->dst_addr);
					}

					free(header);
					free(RRQ);
					free(ether_frame);
				}
			}
		}
				
		for(index=0; index<if_cnt; index++) {

			//RRQ, RRP, or payload received from one of the interface socket
			if(FD_ISSET(if_fd[index], &rset)) {
			
				printf("New Ethernet frame was received from interface %d\n", index+3);

				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
				struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));

				if(recvfrom(if_fd[index], ether_frame, ETH_FRAME_LEN, 0, NULL, NULL)<0) {

					perror("ODR: Failed to receive Ethernet frame througth interface socket\n");
					exit(1);
				}

				memcpy((void *) header, (void *) ether_frame, sizeof(*header));
				
				memcpy(recv_mac, header->src_mac, ETH_ALEN);
				printf("Received Ethernet frame type %d from ", ntohl(header->type));
				ptr=recv_mac;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);

				//RREQ received				
				if(ntohl(header->type)==0) {
				
					printf("\nThe new Ethernet frame is a RREQ\n\n");

					struct RREQ *RRQ=(struct RREQ *) malloc(sizeof(struct RREQ));
					memcpy((void *) RRQ, (void *) (ether_frame+sizeof(*header)), sizeof(*RRQ));
					
					int efficient=0;
					
					//add or update 'reverse' path to source node in routing table
					for(number=0; number<10; number++) {
						
						if(strcmp(RRQ->src_addr, routing_table[number].dst_addr)==0) {
						
							//new route to source is more efficient
							if(ntohl(RRQ->discovery)==1 || routing_table[number].outgoing_if==0 
								|| routing_table[number].hop_cnt>ntohl(RRQ->hop_cnt)+1
								|| (routing_table[number].hop_cnt==ntohl(RRQ->hop_cnt)+1 
									&& routing_table[number].outgoing_if!=index+3)) {
								
								memcpy(routing_table[number].next_hop, header->src_mac, ETH_ALEN);
								routing_table[number].outgoing_if=index+3;
								routing_table[number].hop_cnt=ntohl(RRQ->hop_cnt)+1;
									
								printf("This RREQ gave a better route back to source node VM%d with hop count %d\n", 
											number+1, routing_table[number].hop_cnt);
								printf("Local routing table was updated\n\n");
								efficient=1;					
							}
							
							break;
						}			
					}
					
					//this node is the destination node				
					if(strcmp(RRQ->dst_addr, vm_addr[node-1])==0) {
					
						printf("This node is the destination node for this RREQ\n");
					
						//reply is sent back
						if(ntohl(RRQ->RREP_sent)==0 && (routing_table[number].broadcast_id<ntohl(RRQ->broadcast_id)
							|| (routing_table[number].broadcast_id==ntohl(RRQ->broadcast_id) && efficient==1))) {
							
							struct RREP *RRP=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
							
							//RREP header
							memcpy(header->dst_mac, recv_mac, ETH_ALEN);
							memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
							header->proto=htons(PROTO);
							header->type=htonl(1);
							
							//RREP ODR protocal message
							strcpy(RRP->src_addr, RRQ->dst_addr);
							strcpy(RRP->dst_addr, RRQ->src_addr);
							RRP->dst_port=htonl(RRQ->src_port);
							RRP->hop_cnt=htonl(0);
							RRP->discovery=htonl(ntohl(RRQ->discovery)==1?1:0);
														
							//RREP Ethernet frame
							memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  							memcpy((void *) (ether_frame+sizeof(*header)), (void *) RRP, sizeof(*RRP));
  							printf("RREP is ready to be sent out to ");
  							ptr=header->dst_mac;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_ifindex=index+3;
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, header->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
  							
							if(sendto(if_fd[index], ether_frame, sizeof(*header)+sizeof(*RRP), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								perror("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
							ptr=send_if_addr.sll_addr;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
							
							printf("	ODR message type 1, source ip %s, destination ip %s\n", RRP->src_addr, RRP->dst_addr);
						
							free(RRP);	
						}
	
						routing_table[number].broadcast_id=ntohl(RRQ->broadcast_id);
						
						continue;
					}
					
					//check whether this node has route information to destination node
					for(count=0; count<10; count++) {

						if(strcmp(RRQ->dst_addr, routing_table[count].dst_addr)==0) {
								
							if(routing_table[count].outgoing_if!=0 && ntohl(RRQ->discovery)==0) break;
						}
					}
												
					//this node has route information to destination node
					if(count<10) {
						
						printf("This node has route information to destination\n");
						
						//reply is sent back
						if(ntohl(RRQ->RREP_sent)==0 && (routing_table[number].broadcast_id<ntohl(RRQ->broadcast_id) 
							|| (routing_table[number].broadcast_id==ntohl(RRQ->broadcast_id) && efficient==1))) {
					
							struct RREP *RRP=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
							
							//RREP header
							memcpy(header->dst_mac, recv_mac, ETH_ALEN);
							memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
							header->proto=htons(PROTO);
							header->type=htonl(1);
							
							//RREP ODR protocal message
							strcpy(RRP->src_addr, RRQ->dst_addr);
							strcpy(RRP->dst_addr, RRQ->src_addr);
							RRP->dst_port=htonl(RRQ->src_port);
							RRP->hop_cnt=htonl(routing_table[count].hop_cnt);
							RRP->discovery=htonl(0);
							
							//RREP Ethernet frame
							memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  							memcpy((void *) (ether_frame+sizeof(*header)), (void *) RRP, sizeof(*RRP));
  							printf("RREP is ready to be sent out to ");
  							ptr=header->dst_mac;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_OTHERHOST;
							send_if_addr.sll_ifindex=index+3;	
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, header->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;	
  							
							if(sendto(if_fd[index], ether_frame, sizeof(*header)+sizeof(*RRP), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								perror("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
							ptr=send_if_addr.sll_addr;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
										
							printf("	ODR message type 1, source ip %s, destination ip %s\n", RRP->src_addr, RRP->dst_addr);
							
							free(RRP);
						}
						
						routing_table[number].broadcast_id=ntohl(RRQ->broadcast_id);
						
						//continue flooding the RRQ
						if(efficient==1) {
						
							printf("\nThis node continued to flood RREQ after it sent RREP\n");
						
							RRQ->RREP_sent=htonl(1);
							//hop count plus one
							RRQ->hop_cnt=htonl(ntohl(RRQ->hop_cnt)+1);
							
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
						
							//RREQ header
							header->dst_mac[0]=0xff;		
							header->dst_mac[1]=0xff;
							header->dst_mac[2]=0xff;
							header->dst_mac[3]=0xff;
							header->dst_mac[4]=0xff;
							header->dst_mac[5]=0xff;
							header->proto=htons(PROTO);
							header->type=htonl(0);
						
							memcpy((void *) (ether_frame+sizeof(*header)), RRQ, sizeof(*RRQ));
					
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_halen=ETH_ALEN;
  							send_if_addr.sll_addr[0]=0xff;
  							send_if_addr.sll_addr[1]=0xff;
							send_if_addr.sll_addr[2]=0xff;
							send_if_addr.sll_addr[3]=0xff;
							send_if_addr.sll_addr[4]=0xff;
							send_if_addr.sll_addr[5]=0xff;
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
							
							//flood on all interfaces, except the one that RREQ came in
							for(count=0; count<if_cnt; count++) {
					
								if(count==index) continue;
							
								memcpy(header->src_mac, if_addr[count].sll_addr, ETH_ALEN);
								memcpy((void *) ether_frame, header, sizeof(*header));
							
								send_if_addr.sll_ifindex=count+3;

								if(sendto(if_fd[count], ether_frame, sizeof(*header)+sizeof(*RRQ), 
										0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
										perror("ODR: Failed to send Ethernet frame through interface socket\n");
										exit(1);
								}
							
								printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
										node, node);
								printf("	ODR message type 0, source ip %s, destination ip %s\n", 
										RRQ->src_addr, RRQ->dst_addr);
							}
						}
					}	
					//this node is not destination node and doesn't have route information to destination node
					else {
					
						printf("This node is not the destination node for this RREQ and it doesn't have route to destination\n");
						
						if(routing_table[number].broadcast_id<ntohl(RRQ->broadcast_id) 
							|| (routing_table[number].broadcast_id==ntohl(RRQ->broadcast_id) && efficient==1)) {
						
							printf("RREQ continues to flood\n");
							
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
						
							//RREQ header
							header->dst_mac[0]=0xff;		
							header->dst_mac[1]=0xff;
							header->dst_mac[2]=0xff;
							header->dst_mac[3]=0xff;
							header->dst_mac[4]=0xff;
							header->dst_mac[5]=0xff;
							header->proto=htons(PROTO);
							header->type=htonl(0);
							
							//hop count plus one
							RRQ->hop_cnt=htonl(ntohl(RRQ->hop_cnt)+1);
							
							memcpy((void *) (ether_frame+sizeof(*header)), RRQ, sizeof(*RRQ));
						
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_BROADCAST;
							send_if_addr.sll_halen=ETH_ALEN;
  							send_if_addr.sll_addr[0]=0xff;
  							send_if_addr.sll_addr[1]=0xff;
							send_if_addr.sll_addr[2]=0xff;
							send_if_addr.sll_addr[3]=0xff;
							send_if_addr.sll_addr[4]=0xff;
							send_if_addr.sll_addr[5]=0xff;
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
							
							//flood on all interfaces, except the one that RREQ came in
							for(count=0; count<if_cnt; count++) {
						
								if(count==index) continue;
								
								memcpy(header->src_mac, if_addr[count].sll_addr, ETH_ALEN);
								memcpy((void *) ether_frame, header, sizeof(*header));
								
								send_if_addr.sll_ifindex=count+3;

								if(sendto(if_fd[count], ether_frame, sizeof(*header)+sizeof(*RRQ), 
									0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
									perror("ODR: Failed to send Ethernet frame through interface socket\n");
									exit(1);
								}
								
								printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
										node, node);
								printf("	ODR message type 0, source ip %s, destination ip %s\n", RRQ->src_addr, RRQ->dst_addr);
							}
						}
						
						routing_table[number].broadcast_id=ntohl(RRQ->broadcast_id);
					}
						
					free(RRQ);
				}
				//RREP received
				else if(ntohl(header->type)==1) {
				
					printf("\nThe new Ethernet frame is a RREP\n\n");
				
					struct RREP *RRP=(struct RREP *) malloc(sizeof(struct RREP));
					memcpy(RRP, (void *) (ether_frame+sizeof(*header)), sizeof(*RRP));
					
					//add or update 'forward' path to destination node in routing table
					for(count=0; count<10; count++) {
						
						if(strcmp(RRP->src_addr, routing_table[count].dst_addr)==0) {
						
							//new route to destination is more efficient
							if(ntohl(RRP->discovery)==1 || routing_table[count].outgoing_if==0 
								|| routing_table[count].hop_cnt>ntohl(RRP->hop_cnt)+1
								|| (routing_table[count].hop_cnt==ntohl(RRP->hop_cnt)+1
									&& routing_table[count].outgoing_if!=index+3)) {
								
								memcpy(routing_table[count].next_hop, header->src_mac, ETH_ALEN);
								routing_table[count].outgoing_if=index+3;
								routing_table[count].hop_cnt=ntohl(RRP->hop_cnt)+1;
	
								printf("This RREP gave a better route forward to destination node VM%d with hop count %d\n", 												count+1, routing_table[count].hop_cnt);
								printf("Local routing table was updated\n\n");					
							}
							
							break;
						}			
					}
					
					//this node is destination node
					if(strcmp(RRP->dst_addr, vm_addr[node-1])==0) {
					
						printf("This node is the destination node for this RREP\n");
						
						char data[ETH_FRAME_LEN];
							
						if((cnt=sprintf(data, "%s %s %d", "HI", RRP->src_addr, 5000))<=0) {

							perror("ODR: Failed to format parameters into a single data sequence\n");
							exit(1);
						}
						
						//check which local client is this RREP for
						for(count=0; count<=port_cnt; count++) {
			
							if(port_table[count].port==ntohl(RRP->dst_port)) {

								strcpy(localaddr.sun_path, port_table[count].sun_path);
								break;
							}
						}
						
						if(sendto(odrfd, data, cnt, 0, (SA *) &localaddr, sizeof(localaddr))<0) {
							
							perror("ODR: Failed to send date sequence to local client\n");
							exit(1);
						}
						printf("RREP data sequence was sent to local client\n");
					}
					//this node is not destination node, relaying RREP
					else {
					
						printf("This node is not the destination node for this RREP, and continues relaying this RREP\n");
										
						bzero(ether_frame, ETH_FRAME_LEN);
						bzero(header, sizeof(struct hdr));
							
						//RREP header
						header->proto=htons(PROTO);
						header->type=htonl(1);

						//RREP ODR protocol message
						RRP->hop_cnt=htonl(ntohl(RRP->hop_cnt)+1);
						
						bzero(&send_if_addr, sizeof(struct sockaddr_ll));
						send_if_addr.sll_family=PF_PACKET;
						send_if_addr.sll_protocol=htons(PROTO);
						send_if_addr.sll_halen=ETH_ALEN;
						send_if_addr.sll_addr[6]=0x00;
						send_if_addr.sll_addr[7]=0x00;
					
						for(number=0; number<10; number++) {
						
							if(strcmp(RRP->dst_addr, routing_table[number].dst_addr)==0) {
							
								memcpy(header->dst_mac, routing_table[number].next_hop, ETH_ALEN);
								memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
								memcpy(send_if_addr.sll_addr, routing_table[number].next_hop, ETH_ALEN);
								send_if_addr.sll_ifindex=routing_table[number].outgoing_if;
								break;
							}
						}
						
						//RREP Ethernet frame
						memcpy((void *) ether_frame, (void *) header, sizeof(*header));
						memcpy((void *) (ether_frame+sizeof(*header)), (void *) RRP, sizeof(*RRP));
						printf("RREP is ready to be forwarded to next node ");
						ptr=header->dst_mac;
        				no=ETH_ALEN;
						do {
			
            				printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        				} while(--no>0);
						
						if(sendto(if_fd[routing_table[number].outgoing_if-3], ether_frame, 
								sizeof(*header)+sizeof(*RRP), 0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
							perror("ODR: Failed to send Ethernet frame through interface socket\n");
							exit(1);
						}
								
						printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
						ptr=send_if_addr.sll_addr;
        				no=ETH_ALEN;
						do {
			
            				printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        				} while(--no>0);
									
						printf("	ODR message type 0, source ip %s, destination ip %s\n", RRP->src_addr, RRP->dst_addr);
					}
				
					free(RRP);
				}
				//application payload received
				else {
				
					printf("\nThe new Ethernet frame is an application payload\n\n");
					
					struct payload *load=(struct payload *) malloc(sizeof(struct payload));
					memcpy(load, (void *) (ether_frame+sizeof(*header)), sizeof(*load));
					
					//this node is destination node
					if(strcmp(load->dst_addr, vm_addr[node-1])==0) {
					
						printf("This node is the destination node for this application payload\n");
						
						char data[ETH_FRAME_LEN];
							
						if((cnt=sprintf(data, "%s %s %d", load->message, load->src_addr, load->src_port))<=0) {

							perror("ODR: Failed to format parameters into a single data sequence\n");
							exit(1);
						}
						
						//check which local client/server is this application payload for
						for(count=0; count<=port_cnt; count++) {
			
							if(port_table[count].port==ntohl(load->dst_port)) {

								strcpy(localaddr.sun_path, port_table[count].sun_path);
								break;
							}
						}
						
						if(sendto(odrfd, data, cnt, 0, (SA *) &localaddr, sizeof(localaddr))<0) {
							
							perror("ODR: Failed to send date sequence to local client/server\n");
							exit(1);
						}
						printf("Received data was sent to local client/server\n");
					}
					//this node is not destination node, relaying application payload
					else {
					
						printf("This node is not the destination node for this application payload,");
						printf(" and continue relaying this application payload\n");
					
						bzero(header, sizeof(struct hdr));
						bzero(ether_frame, ETH_FRAME_LEN);
							
						//application payload header
						header->proto=htons(PROTO);
						header->type=htonl(2);
						
						bzero(&send_if_addr, sizeof(struct sockaddr_ll));
						send_if_addr.sll_family=PF_PACKET;
						send_if_addr.sll_protocol=htons(PROTO);
						send_if_addr.sll_hatype=ARPHRD_ETHER;
						send_if_addr.sll_pkttype=PACKET_OTHERHOST;
						send_if_addr.sll_halen=ETH_ALEN;
						send_if_addr.sll_addr[6]=0x00;
						send_if_addr.sll_addr[7]=0x00;
					
						for(number=0; number<10; number++) {
						
							if(strcmp(load->dst_addr, routing_table[number].dst_addr)==0) {
							
								memcpy(header->dst_mac, routing_table[number].next_hop, ETH_ALEN);
								memcpy(header->src_mac, if_addr[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
								memcpy(send_if_addr.sll_addr, routing_table[number].next_hop, ETH_ALEN);
								break;
							}
						}
						
						//application payload Ethernet frame
						memcpy((void *) ether_frame, header, sizeof(*header));
						memcpy((void *) (ether_frame+sizeof(*header)), load, sizeof(*load));
						printf("Application payload is ready to be forwarded to next node ");
						ptr=header->dst_mac;
        				no=ETH_ALEN;
						do {
			
            				printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        				} while(--no>0);
						
						send_if_addr.sll_ifindex=routing_table[number].outgoing_if;
						
						if(sendto(if_fd[routing_table[number].outgoing_if-3], ether_frame, 
								sizeof(*header)+sizeof(*load), 0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
							perror("ODR: Failed to send Ethernet frame through interface socket\n");
							exit(1);
						}
								
						printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
						ptr=send_if_addr.sll_addr;
        				no=ETH_ALEN;
						do {
			
            				printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        				} while(--no>0);
						
						printf("	ODR message type 0, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);
					}
				
					free(load);
				}
				
				free(header);
				free(ether_frame);			  
			}
		}
	}
}

