ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lfqueue

CFLAGS += -Wall -lpthread -DLOCKFREE -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64

.PHONY:	all clean

all:	main

linkedlist.o: lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist.o linkedlist.cc

lfqueue.o: linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lfqueue.o lfqueue.cc

intset.o: linkedlist.h lfqueue.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist.h lfqueue.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc

main: linkedlist.o lfqueue.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) 
	
