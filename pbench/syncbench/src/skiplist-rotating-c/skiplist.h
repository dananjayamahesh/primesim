/*
 * Interface for the skip list data stucture.
 *
 * Author: Ian Dick, 2013
 *
 */
#ifndef SKIPLIST_H_
#define SKIPLIST_H_

#include <atomic_ops.h>

#include "common.h"
#include "ptst.h"
#include "garbagecoll.h"

#define MAX_LEVELS 20

#define NUM_SIZES 1
#define NODE_SIZE 0

#define IDX(_i, _z) ((_z) + (_i)) % MAX_LEVELS
unsigned long sl_zero;

//#define B4 - asplos paper
//#define B96 - 38-41
#define B4

typedef long load_t;

/* bottom-level nodes */
typedef VOLATILE struct sl_node node_t;
struct sl_node {
        unsigned long   level;
        struct sl_node  *prev;
        struct sl_node  *next;
        unsigned long   key;
        void            *val;
        struct sl_node  *succs[MAX_LEVELS];
        unsigned long   marker;
        unsigned long   raise_or_remove;

    #ifdef B4
    load_t load1;
    #endif

    #ifdef B8
    load_t load1;
    load_t load2;
    #endif

    #ifdef B32
    load_t load1;
    load_t load2;
    load_t load3;
    load_t load4;
    load_t load5;
    load_t load6;
    load_t load7;
    load_t load8;
    #endif


    #ifdef B64
    load_t load1;
    load_t load2;
    load_t load3;
    load_t load4;
    load_t load5;
    load_t load6;
    load_t load7;
    load_t load8;
    load_t load9;
    load_t load10;
    load_t load11;
    load_t load12;
    load_t load13;
    load_t load14;
    load_t load15;
    load_t load16;
    #endif

    #ifdef B96
    load_t load1;
    load_t load2;
    load_t load3;
    load_t load4;
    load_t load5;
    load_t load6;
    load_t load7;
    load_t load8;
    load_t load9;
    load_t load10;
    load_t load11;
    load_t load12;
    load_t load13;
    load_t load14;
    load_t load15;
    load_t load16;
    
    load_t load17;
    load_t load18;
    load_t load19;
    load_t load20;
    load_t load21;
    load_t load22;
    load_t load23;
    load_t load24;
    #endif

    #ifdef B256
        load_t load1; __attribute__((aligned(64)));
        load_t load2; __attribute__((aligned(64)));
        load_t load3; __attribute__((aligned(64)));
        load_t load4; __attribute__((aligned(64)));

    #endif

    #ifdef B512
        load_t load1; __attribute__((aligned(64)));
        load_t load2; __attribute__((aligned(64)));
        load_t load3; __attribute__((aligned(64)));
        load_t load4; __attribute__((aligned(64)));
        load_t load5; __attribute__((aligned(64)));
        load_t load6; __attribute__((aligned(64)));
        load_t load7; __attribute__((aligned(64)));
        load_t load8; __attribute__((aligned(64)));
    #endif



};

/* the skip list set */
typedef struct sl_set set_t;
struct sl_set {
        struct sl_node  *head;
};

node_t* node_new(unsigned long key, void *val, node_t *prev, node_t *next,
                 unsigned int level, ptst_t *ptst);

node_t* marker_new(node_t *prev, node_t *next, ptst_t *ptst);

void node_delete(node_t *node, ptst_t *ptst);
void marker_delete(node_t *node, ptst_t *ptst);

set_t* set_new(int start);
void set_delete(set_t *set);
void set_print(set_t *set, int flag);
int set_size(set_t *set, int flag);

void set_subsystem_init(void);
void set_print_nodenums(set_t *set, int flag);

#endif /* SKIPLIST_H_ */
