/*

Micheal scott lock free queue

*/

#include "linkedlist.h"

/* ################################################################### *
 * HARRIS' LINKED LIST
 * ################################################################### */

inline int is_marked_ref(long i);
inline long unset_mark(long i);
inline long set_mark(long i);
inline long get_unmarked_ref(long w);
inline long get_marked_ref(long w);

node_t *lfqueue_search(intset_t *set, val_t val, node_t **left_node);
int lfqueue_find(intset_t *set, val_t val);
int lfqueue_enque(intset_t *set, val_t val);
int lfqueue_deque(intset_t *set, val_t val);
