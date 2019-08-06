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
    node->load1=val+1;
  #endif

  #ifdef B8
    node->load1=val+1;
    node->load2=val+2;
  #endif

  #ifdef B32
    node->load1=val+1;
    node->load2=val+2;
    node->load3=val+3;
    node->load4=val+4;
    node->load5=val+5;
    node->load6=val+6;
    node->load7=val+7;
    node->load8=val+8;
  #endif

  #ifdef B64
    node->load1=val+1;
    node->load2=val+2;
    node->load3=val+3;
    node->load4=val+4;
    node->load5=val+5;
    node->load6=val+6;
    node->load7=val+7;
    node->load8=val+8;
    node->load9=val+9;
    node->load10=val+10;
    node->load11=val+11;
    node->load12=val+12;
    node->load13=val+13;
    node->load14=val+14;
    node->load15=val+15;
    node->load16=val+16;
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
  set->tail = max;

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
