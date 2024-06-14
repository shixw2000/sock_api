#include"shareheader.h"
#include"cache.h"
#include"misc.h"
#include"llist.h"
#include"nodebase.h"


static const int MAX_BUFFER_CNT = 2;

struct NodeMsg {
    LList m_list;
    Buffer m_buffer[MAX_BUFFER_CNT];
    int m_preLen;
    int m_node_type;
    char m_prefix[0];
}; 

NodeMsg* NodeUtil::creatNode(int prelen) {
    NodeMsg* pb = NULL;
    int total = sizeof(NodeMsg) + prelen; 

    pb = (NodeMsg*)CacheUtil::mallocAlign(total);
    if (NULL != pb) {
        MiscTool::bzero(pb, total);
        
        initList(&pb->m_list); 
        pb->m_preLen = prelen;
    } 

    return pb;
}

void NodeUtil::freeNode(NodeMsg* pb) {
    Buffer* buffer = NULL; 

    if (NULL != pb) {
        for (int i=0; i<MAX_BUFFER_CNT; ++i) {
            buffer = &pb->m_buffer[i];

            CacheUtil::freeBuffer(buffer);
        }

        CacheUtil::freeAlign(pb);
    }
}

Buffer* NodeUtil::getBuffer(NodeMsg* pb, bool ext) {
    return &pb->m_buffer[ext]; 
}

char* NodeUtil::getPreNode(NodeMsg* pb, int preLen) {
    if (0 < preLen && preLen == pb->m_preLen) {
        return pb->m_prefix;
    } else {
        return NULL;
    }
}

void NodeUtil::setNodeType(NodeMsg* pb, int type) {
    pb->m_node_type = type;
}

int NodeUtil::getNodeType(NodeMsg* pb) {
    return pb->m_node_type;
}

NodeMsg* NodeUtil::refNode(NodeMsg* pb, int prelen) {
    NodeMsg* ret = NULL;
    Buffer* src = NULL;
    Buffer* dst = NULL;

    ret = creatNode(prelen);
    if (NULL != ret) {
        ret->m_node_type = pb->m_node_type;
        
        for (int i=0; i<MAX_BUFFER_CNT; ++i) {
            dst = &ret->m_buffer[i];
            src = &pb->m_buffer[i];

            dst->m_pos = src->m_pos;
            dst->m_size = src->m_size;
            if (NULL != src->m_cache) {
                dst->m_cache = CacheUtil::ref(src->m_cache);
            } else {
                dst->m_cache = NULL;
            }
        }
    }

    return ret;
}

void NodeUtil::queue(LList* root, NodeMsg* pb) { 
    add(root, &pb->m_list);
}

NodeMsg* NodeUtil::toNode(LList* node) {
    return reinterpret_cast<NodeMsg*>(node);
}

bool NodeUtil::isCompleted(NodeMsg* pb) {
    for (int i=0; i<MAX_BUFFER_CNT; ++i) {
        if (!CacheUtil::isFull(&pb->m_buffer[i])) {
            return false;
        }
    }

    return true;
} 

