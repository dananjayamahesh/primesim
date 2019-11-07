.PHONY:	all

#BENCHS = src/trees/sftree src/linkedlists/lockfree-list src/hashtables/lockfree-ht src/trees/rbtree src/skiplists/sequential
#LBENCHS = src/trees/tree-lock src/linkedlists/lock-coupling-list src/linkedlists/lazy-list src/hashtables/lockbased-ht src/skiplists/skiplist-lock

LFBENCHS = src/lockfree-linkedlist src/lockfree-hashmap src/lockfree-bstree src/skiplist-rotating-c src/lockfree-queue src/lockfree-queue2
LBENCHS = src/lock-coupling-list

#
#LFBENCHS = src/skiplist-rotating-c src/lockfree-queue


#MALLOC=TC

.PHONY:	clean all

all:	lockfree lockbased

lockfree: clean-build
	for dir in $(LFBENCHS); do \
	$(MAKE) "STM=LOCKFREE" -C $$dir; \
	done

lockbased: clean-build
	for dir in $(LBENCHS); do \
	$(MAKE) -C $$dir; \
	done


clean-build: 
	rm -rf build

clean:
	rm -rf build bin
