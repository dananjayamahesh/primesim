ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/locklist

CFLAGS += -Wall -lpthread -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64

.PHONY:	all clean

all:	main

linkedlist-lock.o: lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist-lock.o linkedlist-lock.cc


coupling.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/coupling.o coupling.cc

intset.o: linkedlist-lock.h coupling.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist-lock.h coupling.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_balanced.o test_balanced.cc

main: linkedlist-lock.o coupling.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/coupling.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINDIR)/locklist_regular $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/coupling.o $(BUILDIR)/intset.o $(BUILDIR)/test_balanced.o -o $(BINDIR)/locklist_balanced $(LDFLAGS)

clean:
	rm -f $(BINS)
