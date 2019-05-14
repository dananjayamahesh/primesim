#VERSION = TCMALLOC_NO_UPDATE

ROOT = ../..

include $(ROOT)/common/Makefile.common

.PHONY:	all clean
all:	main

BINS = $(BINDIR)/bstree

CC = g++
CFLAGS += -std=gnu++0x

main: test.o
	$(CC) $(CFLAGS) ${BUILDIR}/test.o -o $(BINDIR)/bstree_regular $(LDFLAGS)
	$(CC) $(CFLAGS) ${BUILDIR}/test_balanced.o -o $(BINDIR)/bstree_balanced $(LDFLAGS)
	$(CC) $(CFLAGS) ${BUILDIR}/test_addonly.o -o $(BINDIR)/bstree_addonly $(LDFLAGS)
	$(CC) $(CFLAGS) ${BUILDIR}/test_mremoved.o -o $(BINDIR)/bstree_mremoved $(LDFLAGS)

test.o: wfrbt.h test.cc test_balanced.cc test_addonly.cc test_mremoved.cc
	$(CC) $(CFLAGS) -c -o ${BUILDIR}/test.o test.cc
	$(CC) $(CFLAGS) -c -o ${BUILDIR}/test_balanced.o test_balanced.cc
	$(CC) $(CFLAGS) -c -o ${BUILDIR}/test_addonly.o test_addonly.cc
	$(CC) $(CFLAGS) -c -o ${BUILDIR}/test_mremoved.o test_mremoved.cc
clean:
	-rm -f $(BINS) *.o $(BUILDIR)
