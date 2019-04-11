ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/skiplist-rotating

CFLAGS += -Wall -lpthread -DLOCKFREE -O
CFLAGS += -m64
LDFLAGS += -m64
CC = g++
#CFLAGS += -std=c++11
ARCH = x86_64

.PHONY:	all clean

all:	main

$(BUILDIR)/ptst.o: ptst.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ptst.o ptst.cc 

$(BUILDIR)/garbagecoll.o: garbagecoll.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/garbagecoll.o garbagecoll.cc 

$(BUILDIR)/nohotspot_ops.o: nohotspot_ops.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/nohotspot_ops.o nohotspot_ops.cc 

$(BUILDIR)/skiplist.o: skiplist.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist.o skiplist.cc

$(BUILDIR)/background.o: background.h skiplist.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/background.o background.cc 

intset.o: intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cc 

test.o: intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cc

main: intset.o $(BUILDIR)/background.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o test.o $(BUILDIR)/ptst.o $(BUILDIR)/garbagecoll.o
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -rf ${BUILDIR}
	-rm -f $(BINS)
