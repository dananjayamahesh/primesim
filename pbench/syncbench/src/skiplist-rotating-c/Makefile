ROOT = ../..

include $(ROOT)/common/Makefile.common
BINDIR=$(ROOT)/bin/
BINS = $(ROOT)/bin/skiplist-rotating-c
BUILDIR = $(ROOT)/build
CC = gcc
CFLAGS += -std=gnu99

.PHONY:	all clean

all:	main

$(BUILDIR)/ptst.o: ptst.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ptst.o ptst.c 

$(BUILDIR)/garbagecoll.o: garbagecoll.h ptst.h common.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/garbagecoll.o garbagecoll.c 

$(BUILDIR)/nohotspot_ops.o: skiplist.h background.h nohotspot_ops.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/nohotspot_ops.o nohotspot_ops.c 

$(BUILDIR)/skiplist.o: skiplist.h background.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist.o skiplist.c 

$(BUILDIR)/background.o: background.h skiplist.h common.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/background.o background.c 

intset.o: intset.h nohotspot_ops.h skiplist.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c 

test.o: intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_balanced.o test_balanced.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_addonly.o test_addonly.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_mremoved.o test_mremoved.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_iterate.o test_iterate.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_backed.o test_backed.c

main: intset.o $(BUILDIR)/background.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o test.o $(BUILDIR)/ptst.o $(BUILDIR)/garbagecoll.o
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test.o -o$(BINDIR)/skiplist_regular $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test_balanced.o -o $(BINDIR)/skiplist_balanced $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test_addonly.o -o $(BINDIR)/skiplist_addonly $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test_mremoved.o -o $(BINDIR)/skiplist_mremoved $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test_iterate.o -o $(BINDIR)/skiplist_iterate $(LDFLAGS)
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test_backed.o -o $(BINDIR)/skiplist_backed $(LDFLAGS)

clean:
	-rm -rf ${BUILDIR}
	-rm -f $(BINS)
	-rm -f $(BINDIR)
	
