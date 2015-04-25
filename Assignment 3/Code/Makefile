CC = gcc

LIBS = /home/yoursong/Documents/unpv13e/libunp.a -lresolv -lnsl -lpthread

FLAGS = -g -O2

CFLAGS = -I/home/yoursong/Documents/unpv13e/lib ${FLAGS} -D_REENTRANT -Wall

all: server_cse533-19 client_cse533-19 ODR_cse533-19

server_cse533-19: server.o API.o get_hw_addrs.o
	${CC} ${FLAGS} -o server_cse533-19 server.o API.o get_hw_addrs.o ${LIBS}
server.o: server.c API.h hw_addrs.h
	${CC} ${CFLAGS} -c server.c API.h hw_addrs.h

client_cse533-19: client.o API.o get_hw_addrs.o
	${CC} ${FLAGS} -o client_cse533-19 client.o API.o get_hw_addrs.o ${LIBS}
client.o: client.c API.h hw_addrs.h
	${CC} ${CFLAGS} -c client.c API.h hw_addrs.h
	
ODR_cse533-19: ODR.o API.o get_hw_addrs.o
	${CC} ${FLAGS} -o ODR_cse533-19 ODR.o API.o get_hw_addrs.o ${LIBS}
ODR.o: ODR.c API.h hw_addrs.h
	${CC} ${CFLAGS} -c ODR.c API.h hw_addrs.h

API.o: API.c
	${CC} ${CFLAGS} -c API.c

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${CFLAGS} -c get_hw_addrs.c

clean:
	rm  server_cse533-19 server.o client_cse533-19 client.o ODR_cse533-19 ODR.o API.o get_hw_addrs.o API.h.gch hw_addrs.h.gch

