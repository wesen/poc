/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#include "conf.h"

#include <stdlib.h>
#include <assert.h>
#include "dlist.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*M
  \emph{Initialize a double-linked head structure.}
**/
void dlist_init(dlist_head_t *head) {
  assert(head != NULL);

  head->num = 0;
  head->dlist = head->end = NULL;
}

/*M
  \emph{Allocate and initialize a double-linked list node structure.}
**/
dlist_t *dlist_new(void *data) {
  dlist_t *dlist = malloc(sizeof(dlist_t));
  
  if (dlist) {
    dlist->next = NULL;
    dlist->prev = NULL;
    dlist->data = data;
  }
  
  return dlist;
}

/*M
  \emph{Free a double-linked list node structure.}
**/
void dlist_free(dlist_t *dlist) {
  assert(dlist != NULL);
  
  free(dlist);
}

/*M
  \emph{Free a double-linked list node structure by calling a custom
  free operation on the nodes data.}
**/
void dlist_delete(dlist_t *dlist,
                  void (*free)(void *data)) {
  assert(dlist != NULL);
  
  if (dlist->data && free)
    free(dlist->data);
  
  dlist_free(dlist);
}

/*M
  \emph{Destroy a complete double-linked list.}

  Calls a custom delete operation on each node in the double-linked list.
**/
void dlist_destroy(dlist_head_t *head,
                   void (*free)(void *data)) {
  assert(head != NULL);
  
  dlist_t *dlist, *next;
  
  head->num = 0;
  head->end = NULL;
  
  if (!(next = head->dlist))
    return;
  
  while (next) {
    dlist = next;
    next = dlist->next;
    dlist_delete(dlist, free);
  }
}

/*M
  \emph{Create a node in front of a node in a double-linked list.}

  Allocates a new double-linked list node structure, initializes it
  with data and inserts it in front of a specific node in the list.
**/
dlist_t *dlist_ins_before(dlist_head_t *head, dlist_t *dlist, void *data) {
  assert(head != NULL);
  assert(dlist != NULL);
  
  dlist_t *tmp = dlist_new(data);
  
  if (!tmp)
    return NULL;
  
  if (dlist_insl_before(head, dlist, tmp)) {
    return tmp;
  } else {
    dlist_delete(tmp, NULL);
    return NULL;
  }
}

/*M
  \emph{Insert a node after a node in a double-linked list.}
  
  Inserts a node after a specific node in the double-linked list.
**/
int dlist_insl_after(dlist_head_t *head, dlist_t *dlist, dlist_t *new) {
  assert(head != NULL);
  assert(dlist != NULL);
  assert(new != NULL);

  new->next   = dlist->next;
  if (new->next)
    new->next->prev = new;
  dlist->next = new;
  new->prev   = dlist;
  
  if (dlist == head->end)
    head->end = new;
  
  head->num++;
  
  return 1;
}

/*M
  \emph{Creates a node after a node in a double-linked list.}

  Allocates a new double-linked list node structure, initializes it
  with data and inserts it in front of a specific node in the list.
**/
dlist_t *dlist_ins_after(dlist_head_t *head, dlist_t *dlist, void *data) {
  assert(head != NULL);
  assert(dlist != NULL);
  
  dlist_t *tmp = dlist_new(data);
  
  if (!tmp)
    return NULL;
  
  if (dlist_insl_after(head, dlist, tmp)) {
    return tmp;
  } else {
    dlist_delete(tmp, NULL);
    return NULL;
  }
}

/*M
  \emph{Insert a node before a node in a double-linked list.}
  
  Inserts a node before a specific node in the double-linked list.
**/
int dlist_insl_before(dlist_head_t *head, dlist_t *dlist, dlist_t *new) {
  if (!head || !dlist || !new)
    return 0;
  
  new->prev   = dlist->prev;
  if (new->prev)
    new->prev->next = new;
  dlist->prev = new;
  new->next   = dlist;
  
  if (dlist == head->dlist)
    head->dlist = new;
  
  head->num++;
  
  return 1;
}

/*M
  \emph{Insert a node at the end of a double-linked list.}
**/
int dlist_insl_end(dlist_head_t *head,
      dlist_t *dlist) {
   if (!head || !dlist)
      return 0;

   if (!head->dlist || !head->end) {
      head->dlist = dlist;
      head->end  = dlist;
      head->num  = 1;
      return 1;
   } else {
      return dlist_insl_after(head, head->end, dlist);
   }
}

/*M
  \emph{Insert a node at the front of a double-linked list.}
**/
int dlist_insl_front(dlist_head_t *head,
      dlist_t *dlist) {
   if (!head || !dlist)
      return 0;

   if (!head->dlist) {
      head->dlist = dlist;
      head->end  = dlist;
      head->num  = 1;
      return 1;
   } else {
      return dlist_insl_before(head, head->dlist, dlist);
   }
}

/*M
  \emph{Creates a new node at the end of a double-linked list.}

  Initializes the node with data.
**/
dlist_t *dlist_ins_end(dlist_head_t *head, void *data) {
   dlist_t *dlist = dlist_new(data);

   if (!dlist)
      return NULL;

   if (dlist_insl_end(head, dlist)) {
      return dlist;
   } else {
      dlist_delete(dlist, NULL);
      return NULL;
   }
}

/*M
  \emph{Creates a new node at the front of a double-linked list.}

  Initializes the node with data.
**/
dlist_t *dlist_ins_front(dlist_head_t *head, void *data) {
   dlist_t *dlist = dlist_new(data);

   if (!dlist)
      return NULL;

   if (dlist_insl_front(head, dlist)) {
      return dlist;
   } else {
      dlist_delete(dlist, NULL);
      return NULL;
   }
}

/*M
  \emph{Pops off a specific node in a double-linked list.}
**/
dlist_t *dlist_getl(dlist_head_t *head, dlist_t *dlist) {
   dlist_t *tmp = head->dlist;
   int     found = 0;

   if (!head || !dlist || !tmp)
      return NULL;

   if (head->dlist == dlist) {
      head->dlist = dlist->next;
      if (head->dlist)
         head->dlist->prev = NULL;

      found = 1;
   }

   if (head->end == dlist) {
      head->end = dlist->prev;
      if (head->end)
         head->end->next = NULL;

      found = 1;
   }

   if (!found) {
      while (tmp->next && (tmp->next != dlist)) {
         tmp = tmp->next;
      }
      
      if (!tmp->next) {
         return NULL;
      } else {
         if ((tmp->next = dlist->next)) {
            tmp->next->prev = tmp;
         }
      }
   }

   head->num--;
   dlist->prev = dlist->next = NULL;
   return dlist;
}
      
/*M
  \emph{Pops off a specific node from a double-linked list and returns
  data.}

  Frees the popped off node.
**/
void *dlist_get(dlist_head_t *head, dlist_t *dlist) {
   dlist_t *tmp = dlist_getl(head, dlist);
   void      *data;

   if (!tmp)
      return NULL;
   else {
      data = tmp->data;
      dlist_free(tmp);
      return data;
   }
}

/*M
  \emph{Pops off the last node in a double-linked list.}
**/
dlist_t *dlist_getl_end(dlist_head_t *head) {
   return dlist_getl(head, head->end);
}

/*M
  \emph{Pops off the front node in a double-linked list.}
**/
dlist_t *dlist_getl_front(dlist_head_t *head) {
   return dlist_getl(head, head->dlist);
}

/*M
  \emph{Returns the data of the last node in a double-linked list.}

  Pops off and frees the last node.
**/
void *dlist_get_end(dlist_head_t *head) {
   return dlist_get(head, head->end);
}

/*M
  \emph{Returns the data of the front node in a double-linked list.}

  Pops off and frees the front node.
**/
void *dlist_get_front(dlist_head_t *head) {
   return dlist_get(head, head->dlist);
}

/*M
  \emph{Returns the data of the first node.}

  Does not pop off the first node.
**/
void *dlist_front(dlist_head_t *head) {
  assert(head != NULL);
  assert(head->dlist != NULL);
  return head->dlist->data;
}

/*M
  \emph{Returns the data of the last node.}

  Does not pop off the first node.
**/
void *dlist_end(dlist_head_t *head) {
  assert(head != NULL);
  assert(head->end != NULL);
  return head->end->data;
}

/*M
  \emph{Search a double-linked list for a matching node.}

  Returns the data of the node for which the dual operand operation
  returns TRUE. Calls the dual operand operation with the nodes data
  and the seed parameter data, passing the seed as first argument.
**/
dlist_t *dlist_search(dlist_head_t *head,
      void *data,
      int (*cmp)(void *data, void *data2)) {
   dlist_t *dlist;

   if (!head || !head->dlist)
      return NULL;

   dlist = head->dlist;

   while (dlist) {
      if (cmp(data, dlist->data))
         return dlist;
      dlist = dlist->next;
   }

   return NULL;
}

/*C
**/

#ifdef DLIST_TEST
#include <stdio.h>

int test_cmp(int *a, int *b) {
   if (*a == *b)
      return 1;

   return 0;
}

void test_free(int *a) {
   free(a);
}

int main(void) {
   dlist_head_t head;
   dlist_t      *list;
   int *a[15], i;

   dlist_init(&head);
   for (i = 0; i < 15; i++) {
      a[i]  = malloc(sizeof(int));
      *a[i] = i;
      if (!dlist_ins_end(&head, a[i]))
         break;
   }

   while ((list = dlist_getl(&head, head.dlist)))
      dlist_delete(list, DLIST_OP(test_free));

   dlist_destroy(&head, DLIST_OP(test_free));

   return 1;
}
#endif /* DLIST_TEST */
