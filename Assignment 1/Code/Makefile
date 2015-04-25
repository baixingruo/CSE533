CC = gcc

LIBS = /home/yoursong/Documents/unpv13e/libunp.a -lresolv -lnsl -lpthread

FLAGS = -g -O2

CFLAGS = -I/home/yoursong/Documents/unpv13e/lib ${FLAGS} -D_REENTRANT -Wall

all: client server echo_cli time_cli 


time_cli: time_cli.o
	${CC} ${FLAGS} -o time_cli time_cli.o ${LIBS}
time_cli.o: time_cli.c
	${CC} ${CFLAGS} -c time_cli.c


echo_cli: echo_cli.o
	${CC} ${FLAGS} -o echo_cli echo_cli.o ${LIBS}
echo_cli.o: echo_cli.c
	${CC} ${CFLAGS} -c echo_cli.c


# server uses the thread-safe version of readline.c

server: tcpechotimesrv.o readline.o
	${CC} ${FLAGS} -o server tcpechotimesrv.o readline.o ${LIBS}
tcpechotimesrv.o: tcpechotimesrv.c
	${CC} ${CFLAGS} -c tcpechotimesrv.c


client: tcpechotimecli.o
	${CC} ${FLAGS} -o client tcpechotimecli.o ${LIBS}
tcpechotimecli.o: tcpechotimecli.c
	${CC} ${CFLAGS} -c tcpechotimecli.c


# pick up the thread-safe version of readline.c from directory "threads"

readline.o: /home/yoursong/Documents/unpv13e/threads/readline.c
	${CC} ${CFLAGS} -c /home/yoursong/Documents/unpv13e/threads/readline.c


clean:
	rm echo_cli echo_cli.o server tcpechotimesrv.o client tcpechotimecli.o time_cli time_cli.o readline.o

