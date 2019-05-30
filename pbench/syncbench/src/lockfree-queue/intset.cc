/*
 *  intset.c
 *  
 *  Integer set operations (contain, insert, delete) 
 *  that call stm-based / lock-free counterparts.
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "intset.h"

int set_contains(intset_t *set, val_t val, int transactional)
{
	int result;	
	result = lfqueue_find(set, val);	
	return result;
}

inline int set_seq_add(intset_t *set, val_t val)
{
	int result;
	node_t *prev, *next;
	
	prev = set->head;
	next = prev->next;
	while (next->val < val) {
		prev = next;
		next = prev->next;
	}
	result = (next->val != val);
	if (result) {
		prev->next = new_node(val, next, 0);
	}
	return result;
}	
		

int set_add(intset_t *set, val_t val, int transactional)
{
	int result;
		result = lfqueue_enque(set, val);	
	return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
	int result = 0;
	result = lfqueue_deque(set, val);	
	return result;
}

void set_print(intset_t *set){
	 node_t * ptr = set->head;
     
	 while(ptr !=NULL){
	 	//print("%d -> " ptr->val);
	 	ptr = ptr->next;
	 }

	 //print("\n");
}
