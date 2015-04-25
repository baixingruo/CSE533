//
//  API.h
//  HW3
//
//  Created by Panchen Xue on 11/10/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include "unpthread.h"

void msg_send(int fd, char* dst_addr, int dst_port, char* sendmsg, int flag);

void msg_recv(int fd, char* recvmsg, char* src_addr, int* src_port);

