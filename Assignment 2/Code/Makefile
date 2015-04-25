# This is a sample Makefile which compiles source files named:
# - tcpechotimeserv.c
# - tcpechotimecli.c
# - time_cli.c
# - echo_cli.c
# and creating executables: "server", "client", "time_cli"
# and "echo_cli", respectively.
#
# It uses various standard libraries, and the copy of Stevens'
# library "libunp.a" in ~cse533/Stevens/unpv13e_solaris2.10 .
#
# It also picks up the thread-safe version of "readline.c"
# from Stevens' directory "threads" and uses it when building
# the executable "server".
#
# It is set up, for illustrative purposes, to enable you to use
# the Stevens code in the ~cse533/Stevens/unpv13e_solaris2.10/lib
# subdirectory (where, for example, the file "unp.h" is located)
# without your needing to maintain your own, local copies of that
# code, and without your needing to include such code in the
# submissions of your assignments.

# Modify it as needed, and include it with your submission.

CC = gcc

LIBS = /home/yoursong/Documents/unpv13e/libunp.a -lresolv -lnsl -lpthread -lm

FLAGS = -g -O2

CFLAGS = -I/home/yoursong/Documents/unpv13e/lib ${FLAGS} -D_REENTRANT -Wall

all: server client

server: server.o get_ifi_info_plus.o CirQueue.o LinkList.o
	${CC} ${FLAGS} -o server server.o get_ifi_info_plus.o CirQueue.o LinkList.o ${LIBS}
server.o: server.c unpifiplus.h CirQueue.h LinkList.h
	${CC} ${CFLAGS} -c server.c unpifiplus.h CirQueue.h LinkList.h

client: client.o get_ifi_info_plus.o CirQueue.o LinkList.o
	${CC} ${FLAGS} -o client client.o get_ifi_info_plus.o CirQueue.o LinkList.o ${LIBS}
client.o: client.c unpifiplus.h CirQueue.h LinkList.h
	${CC} ${CFLAGS} -c client.c unpifiplus.h CirQueue.h LinkList.h

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

CirQueue.o: CirQueue.c
	${CC} ${CFLAGS} -c CirQueue.c

LinkList.o: LinkList.c
	${CC} ${CFLAGS} -c LinkList.c

clean:
	rm  server server.o client client.o get_ifi_info_plus.o CirQueue.o LinkList.o unpifiplus.h.gch CirQueue.h.gch LinkList.h.gch
