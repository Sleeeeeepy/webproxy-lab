# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

echoclient.o: echoclient.c csapp.c csapp.h
	$(CC) $(CFLAGS) -c $^

echoserveri.o: echoserveri.c csapp.c csapp.h
	$(CC) $(CFLAGS) -c $^

echoclient: echoclient.o csapp.o 
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

echoserveri: echoserveri.o csapp.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

echo: echoclient echoserveri

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz
	rm -f echoclient echoserveri
	rm -f *.gch

