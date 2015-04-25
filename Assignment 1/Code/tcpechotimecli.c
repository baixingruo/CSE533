//
//  tcpechotimecli.c
//  HW1
//
//  Created by Panchen Xue on 9/26/14.
//  Copyright (c) 2014 Panchen Xue. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "unpthread.h"

#define SIZE 1024

int main(int argc, char **argv) {
    
    struct in_addr IPaddress_in;
    struct hostent* hostent;
    char str[INET_ADDRSTRLEN];
    char choice[4];
    int pfd[2];
    int pipe_read;
    
    void sig_chld(int);
    
    pid_t pid;
    char buf[SIZE];
    
    if(argc!=2) {
        
        perror("TCPECHOTIMECLI: usage is 'tcpechotimecli <IPaddress/Domain name>'");
        exit(1);
    }
    
    //user input is server IP address in dotted decimal notation
    if(inet_pton(AF_INET, argv[1], &IPaddress_in)) {
        
        if((hostent=gethostbyaddr(&IPaddress_in, 4, AF_INET))!=NULL) {
            
            printf("Official hostname: %s\n", hostent->h_name);
            printf("IP address: %s\n", inet_ntop(AF_INET, &IPaddress_in, str, sizeof(str)));
        }
        else {
            
            perror("TCPECHOTIMECLI: wrong input IP address");
            exit(1);
        }
    }
    //user input is server domain name
    else if((hostent=gethostbyname(argv[1]))!=NULL) {
        
        printf("Official hostname: %s\n", hostent->h_name);
        printf("IP address: %s\n", inet_ntop(AF_INET, *(hostent->h_addr_list), str, sizeof(str)));
    }
    else {
        
        perror("TCPECHOTIMECLI: wrong input hostname or IP address format");
        exit(1);
    }
    
    while(1) {
        
        printf("Which service you want to use, time or echo?\n");
        
        while(1) {
            
            if(scanf("%s", choice)<0 && errno==EINTR) continue;
        
            if(strcmp(choice, "echo")==0) {

				printf("You chose the echo service\n");
				break;
	    	}
 	    	else if(strcmp(choice, "time")==0) {

				printf("You chose the time service\n");
				break;
	    	}	
            else if(strcmp(choice, "quit")==0) {
                
                printf("Bye bye\n");
                exit(0);
            }
            else {
            
                printf("Wrong service choice, please retry\n");
                continue;
            }
        }
        
        //set signal handler for SIGCHLD
        if(signal(SIGCHLD, sig_chld)==SIG_ERR) {
            
            perror("TCPECHOTIMECLI: unable to catch SIGCHLD signal");
            exit(1);
        }
		printf("Signal handler is set for SIGCHLD\n");
        
        //create pipe for communication between parent and child
        if(pipe(pfd)==-1) {
         
            perror("TCPECHOTIMECLI: pipe failed");
            exit(1);
        }

        if((pid=fork())<0) {
                
            perror("TCPECHOTIMECLI: fork failed");
            exit(1);
        }
		printf("Pipe is established and child process is forked\n");
            
        //child process
        if(pid==0) {
            
            close(pfd[0]);

	    	char pipe_port[4];

	    	sprintf(pipe_port, "%d", pfd[1]);
            
            if(strcmp(choice, "time")==0) {
                              
				//execute time service xterm
				if(execlp("xterm", "xterm", "-e", "./time_cli", str, pipe_port, (char*) 0)<0) {
                
                    perror("TCPECHOTIMECLI: unable to execute an xterm");
                    exit(1);
				}	
            }
            else {
                
				//execute echo service xterm
				if(execlp("xterm", "xterm", "-e", "./echo_cli", str, pipe_port, (char*) 0)<0) {

               	    perror("TCPECHOTIMECLI: unable to execute an xterm");
                    exit(1);
				}
            }
        }
        //parent process
        else {

	    	printf("Following is the parent process\n");
            
            close(pfd[1]);

	    	fd_set rset;
            int maxfdp;   

    	    FD_ZERO(&rset);
                
            while(1) {

				FD_SET(fileno(stdin), &rset);
				FD_SET(pfd[0], &rset);

				maxfdp=max(fileno(stdin), pfd[0])+1;

				Select(maxfdp, &rset, NULL, NULL, NULL);

				//user typed in parent window
				if(FD_ISSET(fileno(stdin), &rset)) {

		    		if(Read(fileno(stdin), buf, MAXLINE)>0) {

		    			printf("Please type input at child process terminal\n");
		    			continue;
		    		}
		    		else continue;
				}

				if(FD_ISSET(pfd[0], &rset)) {
                
                    //child process exit normally
                    if((pipe_read=read(pfd[0], buf, SIZE))>0) printf("Child process status: %s\n", buf);
                    else if(pipe_read==0) {
                    
                        printf("Child process close the pipe\n");
                        break;
                    }
                    else if(pipe_read<0 && errno==EINTR) continue;
                    else if(pipe_read<0) {
                    
                    	printf("Child process terminated prematurely\n");
                    	break;
		    		}
                }
            }
            
            close(pfd[0]);
        }
    }
}
               
void sig_chld(int signo) {
    
    pid_t pid;
    int stat;

    pid=wait(&stat);
    printf("child %d terminated\n", pid);
    return;
}

