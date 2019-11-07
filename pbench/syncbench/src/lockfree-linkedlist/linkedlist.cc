/*
 *  linkedlist.c
 *  
 *  Linked list data structure
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "linkedlist.h"

node_t *new_node(val_t val, node_t *next, int transactional)
{
  node_t *node;

  if (transactional) {
	node = (node_t *)MALLOC(sizeof(node_t));
  } else {
	node = (node_t *)malloc(sizeof(node_t));
  }
  if (node == NULL) {
	perror("malloc");
	exit(1);
  }

  node->val = val;
  node->next = next;

    #ifdef B4
    node->load1=(long)val+1;    
    #endif

    #ifdef B8
    node->load1=(long)val+1;    
    node->load2=(long)val+2;  
    #endif

    #ifdef B32
    node->load1=(long)val+1;
    node->load2=(long)val+2;
    node->load3=(long)val+3;
    node->load4=(long)val+4;
    node->load5=(long)val+5;
    node->load6=(long)val+6;
    node->load7=(long)val+7;
    node->load8=(long)val+8;
    #endif

    #ifdef B64
    node->load1=(long)val+1;
    node->load2=(long)val+2;
    node->load3=(long)val+3;
    node->load4=(long)val+4;
    node->load5=(long)val+5;
    node->load6=(long)val+6;
    node->load7=(long)val+7;
    node->load8=(long)val+8;
    node->load9=(long)val+9;
    node->load10=(long)val+10;
    node->load11=(long)val+11;
    node->load12=(long)val+12;
    node->load13=(long)val+13;
    node->load14=(long)val+14;
    node->load15=(long)val+15;
    node->load16=(long)val+16;
    #endif

    #ifdef B96
    node->load1=(long)val+1;
    node->load2=(long)val+2;
    node->load3=(long)val+3;
    node->load4=(long)val+4;
    node->load5=(long)val+5;
    node->load6=(long)val+6;
    node->load7=(long)val+7;
    node->load8=(long)val+8;
    node->load9=(long)val+9;
    node->load10=(long)val+10;
    node->load11=(long)val+11;
    node->load12=(long)val+12;
    node->load13=(long)val+13;
    node->load14=(long)val+14;
    node->load15=(long)val+15;
    node->load16=(long)val+16;

    node->load17=(long)val+17;
    node->load18=(long)val+18;
    node->load19=(long)val+19;
    node->load20=(long)val+20;
    node->load21=(long)val+21;
    node->load22=(long)val+22;
    node->load23=(long)val+23;
    node->load24=(long)val+24;    
    #endif

    #ifdef B256
    node->load1=(long)val+1;
    node->load2=(long)val+2;
    node->load3=(long)val+3;
    node->load4=(long)val+4;
    #endif


    #ifdef B512
    node->load1=(long)val+1;
    node->load2=(long)val+2;
    node->load3=(long)val+3;
    node->load4=(long)val+4;
    node->load5=(long)val+5;
    node->load6=(long)val+6;
    node->load7=(long)val+7;
    node->load8=(long)val+8;
    #endif
    
    #ifdef WRITE_N
    node->write_0 = (long)val+1;
    node->write_1 = (long)val+2;
    node->write_2 = (long)val+3;
    node->write_3 = (long)val+4;
    node->write_4 = (long)val+5;
    node->write_5 = (long)val+6;
    node->write_6 = (long)val+7;
    node->write_7 = (long)val+8;
    #endif

  return node;
}

intset_t *set_new()
{
  intset_t *set;
  node_t *min, *max;
	
  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL, 0);
  min = new_node(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}
