#ifndef __RBTREE_H__
#define __RBTREE_H__
#include"shareheader.h"
#include"llist.h"


struct RbNode {

#define RB_RED 0
#define RB_BLACK 1

    struct RbNode* m_parent;
    struct RbNode* m_left;
    struct RbNode* m_right;
    int m_color;

    /* for debug */
#ifdef __TEST__
    LList m_sib;
    int m_indent;
    int m_val;
#endif
};

struct RbRoot {
    RbNode* m_head;
};

bool isRed(RbNode* node);
void linkRbNode(RbNode* node, RbNode* parent, RbNode** plink);
void insertRbColor(RbNode* node, RbRoot* root);

void eraseRbNode(RbNode* node, RbRoot* root);


#ifdef __TEST__

void printRbTree(RbRoot* root);
bool addRbNode(RbRoot* root, int val);
bool delRbNode(RbRoot* root, int val);
int testRb();

#endif

#endif

