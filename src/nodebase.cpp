#include"shareheader.h"
#include"cache.h"
#include"llist.h"
#include"nodebase.h"


struct NodeInfo {
    Cache* m_cache;
    int m_size;
    int m_pos;
};

struct NodeMsg {
    LList m_list;
    NodeInfo m_info[ENUM_NODE_TYPE_END];
    int m_preLen;
    char m_prefix[0];
}; 

NodeMsg* NodeUtil::creatNode(int prelen) {
    NodeMsg* pb = NULL;
    int total = sizeof(NodeMsg) + prelen; 

    pb = (NodeMsg*)CacheUtil::mallocAlign(total);
    if (NULL != pb) {
        CacheUtil::bzero(pb, total);
        
        initList(&pb->m_list); 
        pb->m_preLen = prelen;
    } 

    return pb;
}

void NodeUtil::freeNode(NodeMsg* pb) {
    NodeInfo* pinfo = NULL; 

    if (NULL != pb) {
        for (int i=0; i<ENUM_NODE_TYPE_END; ++i) {
            pinfo = &pb->m_info[i];

            if (NULL != pinfo->m_cache) {
                CacheUtil::del(pinfo->m_cache);
                pinfo->m_cache = NULL;
            }
        }

        CacheUtil::freeAlign(pb);
    }
}

NodeInfo* NodeUtil::getNodeInfo(
    NodeMsg* pb, int type) {
    if (0 <= type && ENUM_NODE_TYPE_END > type) {
        return &pb->m_info[type];
    } else {
        return NULL;
    } 
}

char* NodeUtil::getPreNode(NodeMsg* pb, int preLen) {
    if (0 < preLen && preLen == pb->m_preLen) {
        return pb->m_prefix;
    } else {
        return NULL;
    }
}

NodeMsg* NodeUtil::refNode(NodeMsg* pb, int prelen) {
    NodeMsg* ret = NULL;
    NodeInfo* p1 = NULL;
    NodeInfo* p2 = NULL;

    ret = creatNode(prelen);
    if (NULL != ret) {
        for (int i=0; i<ENUM_NODE_TYPE_END; ++i) {
            p1 = &ret->m_info[i];
            p2 = &pb->m_info[i];

            p1->m_pos = 0;
            p1->m_size = p2->m_size;
            if (NULL != p2->m_cache) {
                p1->m_cache = CacheUtil::ref(p2->m_cache);
            } else {
                p1->m_cache = NULL;
            }
        }
    }

    return ret;
}

void NodeUtil::setCache(NodeMsg* pb, int type,
    Cache* cache, int size) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        pinfo->m_cache = cache;
        pinfo->m_size = size;
        pinfo->m_pos = 0;
    }
}

void NodeUtil::setSize(NodeMsg* pb,
    int type, int size) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        pinfo->m_size = size;
    }
}

void NodeUtil::setPos(NodeMsg* pb,
    int type, int pos) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        pinfo->m_pos = pos;
    }
}

void NodeUtil::skipPos(NodeMsg* pb,
    int type, int pos) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        pinfo->m_pos += pos;
    }
}

Cache* NodeUtil::getCache(NodeMsg* pb, int type) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        return pinfo->m_cache;
    } else {
        return NULL;
    }
}

int NodeUtil::getSize(NodeMsg* pb, int type) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        return pinfo->m_size;
    } else {
        return 0;
    }
}

int NodeUtil::getPos(NodeMsg* pb, int type) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        return pinfo->m_pos;
    } else {
        return 0;
    }
}

int NodeUtil::getLeft(NodeMsg* pb, int type) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        return pinfo->m_pos < pinfo->m_size ?
            pinfo->m_size - pinfo->m_pos : 0;
    } else {
        return 0;
    }
}

bool NodeUtil::isCompleted(NodeMsg* pb, int type) {
    NodeInfo* pinfo = NULL;

    pinfo = getNodeInfo(pb, type);
    if (NULL != pinfo) {
        return pinfo->m_pos == pinfo->m_size;
    } else {
        return true;
    }
}

void NodeUtil::setInfo(NodeInfo* pinfo,
    Cache* cache, int size, int pos) {
    pinfo->m_cache = cache;
    pinfo->m_size = size;
    pinfo->m_pos = pos;
}

void NodeUtil::queue(LList* root, NodeMsg* pb) { 
    add(root, &pb->m_list);
}

NodeMsg* NodeUtil::toNode(LList* node) {
    return reinterpret_cast<NodeMsg*>(node);
}

