#include"shareheader.h"
#include"nodebase.h"
#include"cache.h"
#include"misc.h"
#include"cmdcache.h"


struct CmdHead_t {
    int m_cmd;
    int m_size;
    char m_body[0];
} __attribute__((aligned(8)));

NodeMsg* CmdUtil::creatNodeCmd(int cmd, int size) {
    NodeMsg* pb = NULL;
    Cache* cache = NULL;
    CmdHead_t* ph = NULL;
    int total = sizeof(CmdHead_t) + size;

    pb = NodeUtil::creatNode(); 
    if (NULL != pb) {
        cache =CacheUtil::alloc(total);
        if (NULL != cache) {
            ph = (CmdHead_t*)CacheUtil::data(cache);

            MiscTool::bzero(ph, total);
            ph->m_cmd = cmd;
            ph->m_size = size;
            
            NodeUtil::setCache(pb, ENUM_NODE_INFR,
                cache, size);
        } else {
            NodeUtil::freeNode(pb);
            pb = NULL;
        } 
    }

    return pb;
}

int CmdUtil::getCmd(NodeMsg* pb) {
    Cache* cache = NULL;
    CmdHead_t* ph = NULL;

    cache = NodeUtil::getCache(pb, ENUM_NODE_INFR);
    if (NULL != cache) {
        ph = (CmdHead_t*)CacheUtil::data(cache);
    }
    
    return ph->m_cmd;
} 

void* CmdUtil::getBody(NodeMsg* pb) {
    Cache* cache = NULL;
    CmdHead_t* ph = NULL;

    cache = NodeUtil::getCache(pb, ENUM_NODE_INFR);
    if (NULL != cache) {
        ph = (CmdHead_t*)CacheUtil::data(cache);
    }
    
    return ph->m_body;
}

