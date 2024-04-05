#include"rbtree.h"


bool isRed(RbNode* node) {
    return NULL != node && RB_RED == node->m_color;
}

static void rotateLeft(RbNode* node, RbRoot* root) {
    RbNode* parent = node->m_parent;
    RbNode* right = node->m_right;

    node->m_right = right->m_left;
    if (NULL != right->m_left) {
        right->m_left->m_parent = node;
    }
    
    right->m_left = node;
    node->m_parent = right;

    right->m_parent = parent;
    if (NULL != parent) {
        if (node == parent->m_left) {
            parent->m_left = right;
        } else {
            parent->m_right = right;
        }
    } else {
        root->m_head = right;
    }
}

static void rotateRight(RbNode* node, RbRoot* root) {
    RbNode* parent = node->m_parent;
    RbNode* left = node->m_left;

    node->m_left = left->m_right;
    if (NULL != left->m_right) {
        left->m_right->m_parent = node;
    }

    left->m_right = node;
    node->m_parent = left;

    left->m_parent = parent;
    if (NULL != parent) {
        if (node == parent->m_left) {
            parent->m_left = left;
        } else {
            parent->m_right = left;
        }
    } else {
        root->m_head = left;
    }
}

void linkRbNode(RbNode* node, RbNode* parent, RbNode** plink) {
    node->m_parent = parent;
    node->m_left = node->m_right = NULL;
    node->m_color = RB_RED;

    *plink = node;
}

/** red-black tree insertion:
*   1. if parent is black, eof;
*   2. if parent is null, go to step 9; 
*   3. parent is red, go to step 4;
*   4. if uncle is red, set both uncle and parent black, 
*       with grandparent setted red. then use grandparent.
*   5. if LL, rotate right, set parent black and gp red, eof;
*   6. if RR, rotae left, set parent black and gp red, eof;
*   7. if LR, rotate left with parent, then rotate right with grandparent, eof;
*   8. if RL, rotate right with parent, then rotate left with grandparent, eof;
*   9. if parent is null, set node black and assign to root;
**/
void insertRbColor(RbNode* node, RbRoot* root) {
    RbNode* parent = NULL;
    RbNode* gparent = NULL;
    RbNode* uncle = NULL;

    parent = node->m_parent;
    while (isRed(parent)) {
        gparent = parent->m_parent;
        if (parent == gparent->m_left) {
            uncle = gparent->m_right;
        } else {
            uncle = gparent->m_left;
        }

        if (NULL != uncle && RB_RED == uncle->m_color) {
            uncle->m_color = RB_BLACK;
            parent->m_color = RB_BLACK; 

            gparent->m_color = RB_RED;
            node = gparent;
            parent = node->m_parent;
            continue;
        } 
        
        if (parent == gparent->m_left) {
            if (node == parent->m_right) {
                rotateLeft(parent, root);

                register RbNode* tmp = node;
                
                node = parent; 
                parent = tmp;
            } 
            
            parent->m_color = RB_BLACK;
            gparent->m_color = RB_RED;
            rotateRight(gparent, root);
        } else {
            if (node == parent->m_left) {
                rotateRight(parent, root);

                register RbNode* tmp = node;

                node = parent;
                parent = tmp;
            }

            parent->m_color = RB_BLACK;
            gparent->m_color = RB_RED;
            rotateLeft(gparent, root);
        }
        
        return;
    } 

    if (NULL == parent) {
        node->m_color = RB_BLACK;
    }
} 

/** red-black tree deletion:
*   1. if parent is null, go to step ;
*   2. if node is red, go to step ;
*   3. if sibling is red, rotate parent by sibling, swap color of both;
*   4. 
**/
static void eraseRbColor(RbNode* node, RbNode* parent, RbRoot* root) {
    RbNode* sibling = NULL;

    while (!isRed(node) && NULL != parent) {
        if (node == parent->m_left) {
            sibling = parent->m_right;
            
            if (RB_RED == sibling->m_color) {
                sibling->m_color = RB_BLACK;
                parent->m_color = RB_RED;
                rotateLeft(parent, root);
                sibling = parent->m_right;
            }

            if (!isRed(sibling->m_left) && !isRed(sibling->m_right)) {
                   sibling->m_color = RB_RED;
                   node = parent;
                   parent = node->m_parent;
            } else {
                if (!isRed(sibling->m_right)) {
                    sibling->m_color = RB_RED;
                    sibling->m_left->m_color = RB_BLACK;
                    rotateRight(sibling, root);
                    sibling = parent->m_right;
                }

                sibling->m_color = parent->m_color;
                parent->m_color = RB_BLACK;
                sibling->m_right->m_color = RB_BLACK;
                rotateLeft(parent, root);
                node = root->m_head;
                break;
            }
            
        } else {
            sibling = parent->m_left;

            if (RB_RED == sibling->m_color) {
                sibling->m_color = RB_BLACK;
                parent->m_color = RB_RED;
                rotateRight(parent, root);
                sibling = parent->m_left;
            }

            if (!isRed(sibling->m_left) && !isRed(sibling->m_right)) {
                sibling->m_color = RB_RED;
                node = parent;
                parent = node->m_parent;
            } else {
                if (!isRed(sibling->m_left)) {
                    sibling->m_color = RB_RED;
                    sibling->m_right->m_color = RB_BLACK;
                    rotateLeft(sibling, root);
                    sibling = parent->m_left;
                }

                sibling->m_color = parent->m_color;
                parent->m_color = RB_BLACK;
                sibling->m_left->m_color = RB_BLACK;
                rotateRight(parent, root);
                node = root->m_head;
                break; 
            }
        }
    }

    if (node) {
        node->m_color = RB_BLACK;
    }
}

void eraseRbNode(RbNode* node, RbRoot* root) {
    RbNode* parent = NULL;
    RbNode* child = NULL;
    RbNode* old = NULL;
    int color = RB_RED;

    if (NULL != node->m_left && NULL != node->m_right) {
        old = node;
        node = node->m_right;
        while (NULL != node->m_left) {
            node = node->m_left;
        } 

        if (NULL != old->m_parent) {
            if (old == old->m_parent->m_left) {
                old->m_parent->m_left = node;
            } else {
                old->m_parent->m_right = node;
            }
        } else {
            root->m_head = node;
        }

        old->m_left->m_parent = node;

        child = node->m_right;
        color = node->m_color;

        if (node != old->m_right) {
            old->m_right->m_parent = node;
            
            parent = node->m_parent; 
            parent->m_left = child;

            if (NULL != child) {
                child->m_parent = parent;
            }
        } else {
            old->m_right = child;
            parent = node;
        }

        node->m_parent = old->m_parent;
        node->m_left = old->m_left;
        node->m_right = old->m_right;
        node->m_color = old->m_color;
    } else {
        if (!node->m_left) {
            child = node->m_right;
        } else {
            child = node->m_left;
        }

        color = node->m_color;
        parent = node->m_parent;
        if (NULL != parent) {
            if (node == parent->m_left) {
                parent->m_left = child;
            } else {
                parent->m_right = child;
            }
        } else {
            root->m_head = child;
        }

        if (NULL != child) {
            child->m_parent = parent;
        }
    }

    if (RB_BLACK == color) {    
        eraseRbColor(child, parent, root);
    }
}


/* for test debug */
#ifdef __TEST__
#include<time.h>
#include<cstdlib>
#include<cstdio>


static const char CHAR_COLOR[] = {'o', '*'};

#define RESET_COLOR "\033[0m"
#define RED_COLOR "\033[0;31;47m"
#define BLACK_COLOR "\033[0;30;47m"

static unsigned long long initLib() {
    unsigned long long ull = time(NULL);
    
    srand(ull);
    return ull;
}

static int randM(int min, int max) {
    int n = 0;
    int mode = max - min;

    if (1 > mode) {
        mode = 1;
    }
    
    n = rand() % mode;
    return min + n;
}

static unsigned long long g_ullTime = initLib();

static int getRbLevel(RbNode* node) {
    int levelL = 0;
    int levelR = 0;
    int level = 0;

    if (NULL != node) {
        if (NULL == node->m_left && NULL == node->m_right) {
            level = 1;
        } else {
            if (NULL != node->m_left) {
                levelL = getRbLevel(node->m_left);
            }

            if (NULL != node->m_right) {
                levelR = getRbLevel(node->m_right);
            }

            level = (levelL > levelR ? levelL : levelR) + 1; 
        }

        return level;
    } else {
        return 0;
    }
}

static void setIndent(RbNode* node, int high) {
    RbNode* parent = node->m_parent;
    int indent = 0;

    indent = (1 << high);
    
    if (NULL != parent) { 
        if (node == parent->m_left) {
            node->m_indent = parent->m_indent - indent;
        } else {
            node->m_indent = parent->m_indent + indent;
        }
    } else {
        node->m_indent = indent;
    }
}

static void setRbIndent(RbNode* node, int high) {
    setIndent(node, high);

    --high;
    if (NULL != node->m_left) {
        setRbIndent(node->m_left, high);
    }

    if (NULL != node->m_right) {
        setRbIndent(node->m_right, high);
    }
}

bool addRbNode(RbRoot* root, int val) {
    RbNode* parent = NULL;
    RbNode* curr = NULL;
    RbNode** pnode = &root->m_head;

    if (NULL != root->m_head) { 
        do {
            parent = *pnode;
            
            if (val < parent->m_val) {
                pnode = &parent->m_left;
            } else if (val > parent->m_val) {
                pnode = &parent->m_right;
            } else {
                return false;
            }
        } while (NULL != *pnode);
    }

    curr = new RbNode;

    curr->m_val = val;
    linkRbNode(curr, parent, pnode);

    insertRbColor(curr, root); 
    return true;
}

bool delRbNode(RbRoot* root, int val) {
    RbNode* curr = NULL;

    curr = root->m_head;
    while (NULL != curr) { 
        if (val < curr->m_val) {
            curr = curr->m_left;
        } else if (val > curr->m_val) {
            curr  = curr->m_right;
        } else {
            break;
        }
    } 

    if (NULL != curr) {
        eraseRbNode(curr, root);
        return true;
    } else {
        return false;
    }
}

static void printNodeList(LList* root, LList* rootN) {
    RbNode* node = NULL;
    LList* curr = root->m_next;
    int offset = 0;
    int width = 0; 
    
    while (root != curr) {
        node = CONTAINER(curr, RbNode, m_sib);

        if (NULL != node->m_left) {
            add(rootN, &node->m_left->m_sib);
        }

        if (NULL != node->m_right) {
            add(rootN, &node->m_right->m_sib);
        }

        width = node->m_indent - offset;
        width -= 1;

        printf("%*s", width * 2, "");
        if (RB_RED == node->m_color) {
            printf(RED_COLOR "%2d" RESET_COLOR, node->m_val);
        } else {
            printf(BLACK_COLOR "%2d" RESET_COLOR, node->m_val);
        }

        offset = node->m_indent;
        curr = curr->m_next;
    }

    printf("\n");
}

void printRbTree(RbRoot* root) {
    LList sibs[2];
    RbNode* node = NULL;
    int level = 0;
    int high = 0;
    int curr = 0;
    
    if (NULL == root || NULL == root->m_head) {
        return;
    }

    node = root->m_head;
    level = getRbLevel(node);

    high = level - 1;
    setRbIndent(node, high);

    initList(&sibs[0]);
    initList(&sibs[1]);
    
    add(&sibs[curr], &node->m_sib);
  
    while (!isEmpty(&sibs[curr])) { 
        initList(&sibs[!curr]);
        
        printNodeList(&sibs[curr], &sibs[!curr]);

        curr = !curr;
    } 
}

static int testRb1() {
    int ret = 0;
    int val = 0;
    RbRoot root = { NULL };

    printf("enter a val: ");
    scanf("%d", &val);
        
    while (0 != val) {
        if (0 < val) {
            addRbNode(&root, val);
        } else {
            delRbNode(&root, -val);
        }
        
        printf("_________\n");
        printRbTree(&root);
        
        printf("enter a val: ");
        scanf("%d", &val);
    }

    return ret;
}

static int testRb2() {
    bool bOk = false;
    int ret = 0;
    int val = 0;
    int n = 0;
    RbRoot root = { NULL };

    printf("enter a random num of val: ");
    scanf("%d", &n);
        
    while (0 < n) {
        val = randM(-99, 99);
        
        if (0 < val) {
            bOk = addRbNode(&root, val);
        } else {
            bOk = delRbNode(&root, -val);
        }

        if (bOk) {
            printf("____input: %d_________\n", val);
            printRbTree(&root);
            printf("----------------\n");
            
            --n;
        }
    }

    return ret;
}

int testRb() {
    testRb1();
    testRb2();

    return 0;
}

#endif

