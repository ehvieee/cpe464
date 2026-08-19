# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o handles.o functions.o pollLib.o safeUtil.o srPDU.o

all:   cclient server testHandles manyHandles

cclient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.c  $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

testHandles: testHandles.c handles.o
	$(CC) $(CFLAGS) -o $@ testHandles.c handles.c $(LIBS)

manyHandles: manyHandles.c $(OBJS)
	$(CC) $(CFLAGS) -o manyHandles manyHandles.c  $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient testHandles *.o




