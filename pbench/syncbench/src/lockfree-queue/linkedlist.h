/*
 *  linkedlist.h
 *  
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>


#include "lockfree.h"
//#include "release_atomic.h"

#ifdef DEBUG
#define IO_FLUSH                        fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY							4
#define DEFAULT_ALTERNATE								0
#define DEFAULT_EFFECTIVE								1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB(a, e, v)          (AO_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_CAS_MB_REL(a, e, v)          (AO_compare_and_swap_rel((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_CAS_MB_ACQ(a, e, v)          (AO_compare_and_swap_acq((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_CAS_MB_NOB(a, e, v)          (AO_compare_and_swap_nob((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))

#define AO_nop_full() 		__asm__ __volatile__("mfence" : : : "memory");		

static volatile AO_t stop;

#define TRANSACTIONAL                   d->unit_tx

#define MAX_PARITY_LOAD 100
//#define EPOCH_PARITY
//#define WRITES_N

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX
typedef long load_t;

typedef struct node {
	val_t val;
	load_t load1;
	load_t load2;
	struct node *next;
} node_t;

typedef struct intset {
	node_t *head;
	node_t *tail; //tail
} intset_t;

node_t *new_node(val_t val, node_t *next, int transactional);
intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);