ROOT = ../..

include $(ROOT)/common/Makefile.common
#longer version of the micheal scott queue - serach through the list until tail to 
BINS = $(BINDIR)/lfqueue2

CFLAGS += -Wall -lpthread -DLOCKFREE -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64

.PHONY:	all clean

all:	main

linkedlist.o: lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist.o linkedlist.cc

lfqueue2.o: linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lfqueue2.o lfqueue2.cc

intset.o: linkedlist.h lfqueue2.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist.h lfqueue2.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_balanced.o test_balanced.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_addonly.o test_addonly.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_mremoved.o test_mremoved.cc

main: linkedlist.o lfqueue2.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINDIR)/lfqueue2_regular $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_balanced.o -o $(BINDIR)/lfqueue2_balanced $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_addonly.o -o $(BINDIR)/lfqueue2_addonly $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/lfqueue.o $(BUILDIR)/intset.o $(BUILDIR)/test_mremoved.o -o $(BINDIR)/lfqueue2_mremoved $(LDFLAGS)

clean:
	-rm -f $(BINS) 
	
