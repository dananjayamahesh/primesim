ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lock-coupling-list

CFLAGS += -Wall -lpthread -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64

.PHONY:	all clean

all:	main

linkedlist-lock.o: lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist-lock.o linkedlist-lock.cc

lazy.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lazy.o lazy.cc

coupling.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/coupling.o coupling.cc

intset.o: linkedlist-lock.h coupling.h lazy.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist-lock.h coupling.h lazy.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc

main: linkedlist-lock.o coupling.o lazy.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/lazy.o $(BUILDIR)/coupling.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	rm -f $(BINS)
