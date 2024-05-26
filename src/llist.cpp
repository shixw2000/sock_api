#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"llist.h"
#include"cache.h"
#include"misc.h"


void initList(LList* list) {
    list->m_prev = list->m_next = list;
}

static void _nodeAdd(LList* node, LList* prev, LList* next) {
    node->m_prev = prev;
    node->m_next = next;
    
    prev->m_next = node;
    next->m_prev = node;
}

void add(LList* root, LList* node) {
    _nodeAdd(node, root->m_prev, root);
}

void addFront(LList* root, LList* node) {
    _nodeAdd(node, root, root->m_next);
}

bool isEmpty(LList* node) { 
    return node->m_next == node;
}

void del(LList* node) {
    if (!isEmpty(node)) {
        node->m_next->m_prev = node->m_prev;
        node->m_prev->m_next = node->m_next;

        initList(node);
    }
}

LList* first(LList* root) {
    if (root != root->m_next) {
        return root->m_next;
    } else {
        return NULL;
    }
}

static void _splice(LList* from, LList* prev, LList* next) {
    LList* first = from->m_next;
    LList* last = from->m_prev;

    first->m_prev = prev;
    prev->m_next = first;
    
    last->m_next = next;
    next->m_prev = last;
}

void append(LList* to, LList* from) {
    if (!isEmpty(from)) {
        _splice(from, to->m_prev, to);

        initList(from);
    }
}


/* hlist */
void initHList(HList *h) {
    h->m_next = NULL;
    h->m_pprev = NULL;
}

void initHRoot(HRoot *h) {
    h->m_first = NULL;
}

int hlistUnhashed(const HList* node) {
	return !node->m_pprev;
}

int hlistEmpty(const HRoot* root) {
	return !root->m_first;
}

HList* hlistFirst(HRoot* root) {
    return root->m_first;
}

void hlistDel(HList* node) {
	HList* next = node->m_next;
	HList** pprev = node->m_pprev;
    
	*pprev = next;
	if (next) {
		next->m_pprev = pprev;
	}

    initHList(node);
}

void hlistAdd(HRoot* root, HList* node) {
	HList* first = root->m_first;
	
	if (first) {
		first->m_pprev = &node->m_next;
	}

    node->m_next = first;
    node->m_pprev = &root->m_first;
    
	root->m_first = node; 
}

void hlistReplace(HRoot* dst, HRoot* src) {
	dst->m_first = src->m_first;
    
	if (NULL != dst->m_first) {
		dst->m_first->m_pprev = &dst->m_first;
	}
    
    initHRoot(src);
}


/* here is a queue */
void initQue(Queue* queue) {
    MiscTool::bzero(queue, sizeof(Queue));
}

bool creatQue(Queue* queue, int cap) {
    ++cap;
    
    queue->m_cache = (void**)CacheUtil::callocAlign(
        cap, sizeof(void*));
    if (NULL != queue->m_cache) {
        queue->m_cap = cap; 
        queue->m_head = queue->m_tail = 0;
        return true;
    } else {
        return false;
    }
}

void finishQue(Queue* queue) {
    if (NULL != queue->m_cache) {
        CacheUtil::freeAlign(queue->m_cache);
    }

    MiscTool::bzero(queue, sizeof(Queue));
}

bool isQueEmpty(Queue* queue) {
    return queue->m_tail == queue->m_head;
}

bool isQueFull(Queue* queue) {
    int next = queue->m_tail + 1;

    if (next >= queue->m_cap) {
        next -= queue->m_cap;
    }

    return next == queue->m_head;
}

bool push(Queue* queue, void* node) {
    int next = queue->m_tail + 1;

    if (next >= queue->m_cap) {
        next -= queue->m_cap;
    }

    if (next != queue->m_head) {
        queue->m_cache[queue->m_tail] = node;
        queue->m_tail = next;

        return true;
    } else {
        return false;
    }
}

bool pop(Queue* queue, void** ret) {
    if (!isQueEmpty(queue)) {
        *ret = queue->m_cache[queue->m_head];

        ++queue->m_head;
        if (queue->m_head >= queue->m_cap) {
            queue->m_head -= queue->m_cap;
        }

        return true;
    } else {
        *ret = NULL;
        return false;
    }
}


