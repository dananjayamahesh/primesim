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
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_balanced.o test_balanced.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_addonly.o test_addonly.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_mremoved.o test_mremoved.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_iterate.o test_iterate.cc

main: linkedlist.o lfqueue.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINDIR)/lfqueue_regular $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_balanced.o -o $(BINDIR)/lfqueue_balanced $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_addonly.o -o $(BINDIR)/lfqueue_addonly $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_mremoved.o -o $(BINDIR)/lfqueue_mremoved $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_iterate.o -o $(BINDIR)/lfqueue_iterate $(LDFLAGS)

clean:
	-rm -f $(BINS) 
	
