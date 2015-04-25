CC = gcc

LIBS = /home/yoursong/Documents/unpv13e/libunp.a -lresolv -lnsl -lpthread

FLAGS = -g -O2

CFLAGS = -I/home/yoursong/Documents/unpv13e/lib ${FLAGS} -D_REENTRANT -Wall

all: tour arp

tour: tour.o get_hw_addrs.o
	${CC} ${FLAGS} -o tour tour.o get_hw_addrs.o ${LIBS}
tour.o: tour.c hw_addrs.h
	${CC} ${CFLAGS} -c tour.c hw_addrs.h

arp: arp.o get_hw_addrs.o
	${CC} ${FLAGS} -o arp arp.o get_hw_addrs.o ${LIBS}
arp.o: arp.c hw_addrs.h
	${CC} ${CFLAGS} -c arp.c hw_addrs.h

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${CFLAGS} -c get_hw_addrs.c

clean:
	rm  tour tour.o arp arp.o get_hw_addrs.o hw_addrs.h.gch

