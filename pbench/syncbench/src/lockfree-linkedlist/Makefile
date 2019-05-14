ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/linkedlist

CFLAGS += -Wall -lpthread -DLOCKFREE -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64

.PHONY:	all clean

all:	main

linkedlist.o: lockfree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist.o linkedlist.cc

harris.o: linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/harris.o harris.cc

intset.o: linkedlist.h harris.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc

test_balanced.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_balanced.o test_balanced.cc

test_addonly.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_addonly.o test_addonly.cc

test_mremoved.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_mremoved.o test_mremoved.cc

main: linkedlist.o harris.o intset.o test.o test_balanced.o test_addonly.o test_mremoved.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINDIR)/linkedlist_regular $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/intset.o $(BUILDIR)/test_balanced.o -o $(BINDIR)/linkedlist_balanced $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/intset.o $(BUILDIR)/test_addonly.o -o $(BINDIR)/linkedlist_addonly $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/intset.o $(BUILDIR)/test_mremoved.o -o $(BINDIR)/linkedlist_mremoved $(LDFLAGS)

clean:
	-rm -f $(BINS) $(BUILDIR)
	
