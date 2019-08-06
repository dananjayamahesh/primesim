/*

 */

#include "lfqueue.h"

/*
 * The five following functions handle the low-order mark bit that indicates
 * whether a node is logically deleted (1) or not (0).
 *  - is_marked_ref returns whether it is marked, 
 *  - (un)set_marked changes the mark,
 *  - get_(un)marked_ref sets the mark before returning the node.
 */
inline int is_marked_ref(long i) {
    return (int) (i & (LONG_MIN+1));
}

inline long unset_mark(long i) {
	i &= LONG_MAX-1;
	return i;
}

inline long set_mark(long i) {
	i = unset_mark(i);
	i += 1;
	return i;
}

inline long get_unmarked_ref(long w) {
	return unset_mark(w);
}

inline long get_marked_ref(long w) {
	return set_mark(w);
}


node_t *lfqueue_search(intset_t *set, val_t val, node_t **left_node) {
	node_t *left_node_next, *right_node;
	left_node_next = set->head;

	//printf("Search \n");
	
search_again:
	do {
		node_t *t = set->head;
		node_t *t_next = set->head->next;
		
		/* Find left_node and right_node */
		do {
			if (!is_marked_ref((long) t_next)) {
				(*left_node) = t;
				left_node_next = t_next;
			}
			t = (node_t *) get_unmarked_ref((long) t_next);
			if (!t->next) break;
			t_next = t->next;
		} while (is_marked_ref((long) t_next) || (t->val < val));
		right_node = t;
		
		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		}
		//printf("searching CAS %d\n",(int) val);
		/* Remove one or more marked nodes */
		if (ATOMIC_CAS_MB(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		} 
		
	} while (1);
}

//search function has never been called
int lfqueue_find(intset_t *set, val_t val) {
	
	node_t * ptr = set->head;

	//printf("Find \n");

	while(ptr !=NULL){
		if(ptr->val == val) return 1;

		ptr = ptr->next;
		
	}
	return 0;
}

/*
int lfqueue_enque(intset_t *set, val_t val) {
	//printf("insert value - %d\n", (int) val);
	node_t *newnode, *tail, *next;
	newnode = new_node(val, NULL, 0);
	
	do {

		tail = set->tail;
		next = tail->next;	

		if(tail == set->tail){
			if(next == NULL){
				if(ATOMIC_CAS_MB(&tail->next, next, newnode))
					break;
			}
			else{
				ATOMIC_CAS_MB(&set->tail, tail, next);
			}
		}	
		
	} while(1);

	ATOMIC_CAS_MB(&set->tail, tail, newnode);
	return 1;

}

int lfqueue_deque(intset_t *set, val_t val) { //Does not have to do with the value.
	//printf("insert value - %d\n", (int) val);
	node_t *head, *tail, *next;
		
	do {
		head = set->head;
		tail = set->tail;
		next = head->next;

		if(head == set->head){
			if(head == tail){
				if(next == NULL)
					return 0;
				else ATOMIC_CAS_MB(set->tail, tail, next);

			}
			else{
				//Read value befire CAS
				//*pvalue = next->value;
				if(ATOMIC_CAS_MB(&set->head, head, next)) break;

			}
		}		

	} while(1);

	free(head);
	return 1;
}

*/
///////////////////////////////////////////////////////////////////////////////////////
////Non MC queue

//Enque a node to the head.
int lfqueue_enque(intset_t *set, val_t val) {
	//printf("insert value - %d\n", (int) val);
	node_t *newnode, *tail, *next, *head;
	//printf("Enqueue %d \n", val);

	do{

		head = set->head;
		newnode = new_node(val, set->head, 0);
		//newnode->load1=(long)val+1;
		//newnode->load2=(long)val+2;		
		
		if(ATOMIC_CAS_MB(&set->head, head, newnode))
			return 1;
		//or return 1;

		
	}while(1);

	free(head);
	return 1;

}

int lfqueue_deque(intset_t *set, val_t val) { //Does not have to do with the value.
	//printf("insert value - %d\n", (int) val);
	node_t *head, *tail, *next, *ptr, *left;
	//printf("Dequeue %d \n", val);
		
	//do {
		
		ptr = set->head;
		left = NULL; //Header pointer - 1

		if(ptr == NULL) return 0;

		//Searching for the tail
		while(ptr->next != NULL){
			left = ptr;
			ptr = ptr->next;
		} 

		if(ptr == head){
			//left==NULL
			if(ATOMIC_CAS_MB(&set->head, ptr, NULL))
			return 1;
		}
		else{
			if(ATOMIC_CAS_MB(&left->next, ptr, NULL))
			return 1;
		}

		//endedup in the tail ptr=tail
		//for single element - if not 1

		//deque

	//} while(1);

	//free(ptr);
	//free(left);


	return 0;
}