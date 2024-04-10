#ifndef __LLIST_H__
#define __LLIST_H__


struct LList {
    struct LList* m_next;
    struct LList* m_prev;
};

#define INIT_LIST(name) {&(name), &(name)}

void initList(LList* list); 
bool isEmpty(LList* node); 
void del(LList* node); 
LList* first(LList* root); 

void add(LList* root, LList* node); 
void addFront(LList* root, LList* node);
void append(LList* to, LList* from);

#define for_each_list(node, root) \
    for (node = (root)->m_next; node != (root); node = node->m_next)

#define for_each_list_safe(node, n, root) \
    for (node=(root)->m_next;  \
        node != (root) && ({n=node->m_next; true;}); node=n)


/* hlist here */
struct HList {
    struct HList* m_next;
    struct HList** m_pprev;
};

struct HRoot {
    struct HList* m_first;
};

#define INIT_HLIST_NODE {NULL, NULL}
#define INIT_HLIST_HEAD {NULL}

void initHList(HList* node);
void initHRoot(HRoot* root);
int hlistUnhashed(const HList* node);
int hlistEmpty(const HRoot* root);
HList* hlistFirst(HRoot* root);
void hlistDel(HList* node);
void hlistAdd(HRoot* root, HList* node);
void hlistReplace(HRoot* dst, HRoot* src);

#define for_each_hlist(pos, head) \
	for (pos = (head)->m_first; pos; pos = pos->m_next)
        
#define for_each_hlist_safe(pos, n, head) \
	for (pos=(head)->m_first; pos && ({n=pos->m_next; true;}); pos=n)


/* here is an other queue */
struct Queue {
    void** m_cache;
    int m_head;
    int m_tail;
    int m_cap;
};

void initQue(Queue* queue);
bool isQueEmpty(Queue* queue);
bool isQueFull(Queue* queue);
bool creatQue(Queue* queue, int cap);
void finishQue(Queue* queue);
bool push(Queue* queue, void* node);
bool pop(Queue* queue, void** ret);

#endif

