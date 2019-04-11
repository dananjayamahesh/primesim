ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/hashmap

CFLAGS += -Wall -lpthread -DLOCKFREE -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
ARCH = x86_64
#CFLAGS += -I$(SRCDIR) 
LLREP = ../lockfree-linkedlist

.PHONY:	all clean

all:	main

linkedlist.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist.o $(LLREP)/linkedlist.cc

harris.o: $(LLREP)/linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/harris.o $(LLREP)/harris.cc

ll-intset.o: $(LLREP)/linkedlist.h linkedlist.o harris.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ll-intset.o $(LLREP)/intset.cc

hashtable.o: $(LLREP)/linkedlist.h linkedlist.o harris.o ll-intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/hashtable.o hashtable.cc

intset.o: $(LLREP)/linkedlist.h harris.o hashtable.o 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc

test.o: linkedlist.o harris.o intset.o hashtable.o intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc

main: linkedlist.o harris.o intset.o hashtable.o intset.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/ll-intset.o $(BUILDIR)/hashtable.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)