/*C
  (c) 2003 Institut fuer Telematik, Universitaet Karlsruhe
**/

#ifndef DLIST_H__
#define DLIST_H__

/*M
  \emph{Double-linked list node structure.}
**/
typedef struct dlist_s {
   void             *data;
   struct dlist_s *next;
   struct dlist_s *prev;
} dlist_t;

/*M
  \emph{Cast to a single operand list operation.}
**/
#define DLIST_OP(f) (void (*)(void *))(f)
/*M
  \emph{Cast to a dual operand list operation.}
**/
#define DLIST_OP2(f) (int (*)(void *, void *))(f)

/*M
  \emph{Double-linked list head structure.}
**/
typedef struct dlist_head_s {
   struct dlist_s *dlist;
   struct dlist_s *end;
   unsigned int     num;
} dlist_head_t;

/*C
**/

void dlist_init(dlist_head_t *head);

dlist_t *dlist_new(void *data);
void dlist_free(dlist_t *dlist);
void dlist_delete(dlist_t *dlist,
      void (*free)(void *));
void dlist_destroy(dlist_head_t *head,
      void (*free)(void *data));

int dlist_insl_after(dlist_head_t *head, 
                     dlist_t *dlist, 
                     dlist_t *new);
int dlist_insl_before(dlist_head_t *head, 
                      dlist_t *dlist, 
                      dlist_t *new);
dlist_t *dlist_ins_after(dlist_head_t *head, 
                         dlist_t *dlist, 
                         void *data);
dlist_t *dlist_ins_before(dlist_head_t *head, 
                          dlist_t *dlist, 
                          void *data);
int dlist_insl_end(dlist_head_t *head, dlist_t *dlist);
int dlist_insl_front(dlist_head_t *head, dlist_t *dlist);
dlist_t *dlist_ins_end(dlist_head_t *head, void *data);
dlist_t *dlist_ins_front(dlist_head_t *head, void *data);

dlist_t *dlist_getl(dlist_head_t *head, dlist_t *dlist);
void *dlist_get(dlist_head_t *head, dlist_t *dlist);
dlist_t *dlist_getl_end(dlist_head_t *head);
dlist_t *dlist_getl_front(dlist_head_t *head);
void *dlist_get_end(dlist_head_t *head);
void *dlist_get_front(dlist_head_t *head);
void *dlist_front(dlist_head_t *head);
void *dlist_end(dlist_head_t *head);

dlist_t *dlist_search(dlist_head_t *head,
      void *data,
      int (*cmp)(void *data, void *data2));

#define dlist_push(h, x) dlist_ins_front((h), (x))
#define dlist_pop(h) dlist_get_front((h))

#endif /* DLIST_H__ */
