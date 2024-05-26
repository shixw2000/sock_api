#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"msgtool.h"
#include"nodebase.h"
#include"cache.h"
#include"misc.h"
#include"socktool.h"
 

NodeMsg* MsgTool::allocMsg(int size, int prelen) {
    NodeMsg* pb = NULL;
    Cache* cache = NULL;
    char* psz = NULL;

    pb = NodeUtil::creatNode(prelen); 
    if (NULL != pb) {
        cache =CacheUtil::alloc(size);
        if (NULL != cache) {
            psz = CacheUtil::data(cache); 
            MiscTool::bzero(psz, size);
            
            NodeUtil::setCache(pb, ENUM_NODE_INFR,
                cache, size);
        } else {
            NodeUtil::freeNode(pb);
            pb = NULL;
        } 
    }

    return pb;
}

void MsgTool::freeMsg(NodeMsg* pb) {
    NodeUtil::freeNode(pb);
} 

NodeMsg* MsgTool::refNodeMsg(NodeMsg* pb, int prelen) {
    return NodeUtil::refNode(pb, prelen);
}

void MsgTool::setInfraCb(NodeMsg* pb, PFree pf) {
    Cache* cache = NULL;

    cache = NodeUtil::getCache(pb, ENUM_NODE_INFR);
    if (NULL != cache) {
        CacheUtil::setCb(cache, pf);
    }
}

char* MsgTool::getMsg(NodeMsg* pb) {
    Cache* cache = NULL;
    char* psz = NULL;

    cache = NodeUtil::getCache(pb, ENUM_NODE_INFR);
    if (NULL != cache) {
        psz = CacheUtil::data(cache);
    }
    
    return psz;
}

char* MsgTool::getExtraMsg(NodeMsg* pb) {
    Cache* cache = NULL;
    char* psz = NULL;

    cache = NodeUtil::getCache(pb, ENUM_NODE_VAR);
    if (NULL != cache) {
        psz = CacheUtil::data(cache);
    }
    
    return psz;
} 

bool MsgTool::completedMsg(NodeMsg* pb) {
    return NodeUtil::isCompleted(pb, ENUM_NODE_INFR)
        && NodeUtil::isCompleted(pb, ENUM_NODE_VAR);
}

char* MsgTool::getPreNode(NodeMsg* pb, int preLen) {
    return NodeUtil::getPreNode(pb, preLen);
}

void MsgTool::setMsgSize(NodeMsg* pb, int size) {
    NodeUtil::setSize(pb, ENUM_NODE_INFR, size); 
}

int MsgTool::getMsgSize(NodeMsg* pb) {
    return NodeUtil::getSize(pb, ENUM_NODE_INFR);
}

int MsgTool::getMsgPos(NodeMsg* pb) {
    return NodeUtil::getPos(pb, ENUM_NODE_INFR);
}

void MsgTool::setMsgPos(NodeMsg* pb, int pos) {
    NodeUtil::setPos(pb, ENUM_NODE_INFR, pos);
}

void MsgTool::skipMsgPos(NodeMsg* pb, int pos) {
    NodeUtil::skipPos(pb, ENUM_NODE_INFR, pos);
} 

void MsgTool::setExtraCache(NodeMsg* pb, 
    Cache* cache, int size) {
    NodeUtil::setCache(pb, ENUM_NODE_VAR, cache, size);
}

Cache* MsgTool::getExtraCache(NodeMsg* pb) {
    return NodeUtil::getCache(pb, ENUM_NODE_VAR);
}
       
void MsgTool::setExtraSize(NodeMsg* pb, int size) {
    NodeUtil::setSize(pb, ENUM_NODE_VAR, size); 
}

int MsgTool::getExtraSize(NodeMsg* pb) {
    return NodeUtil::getSize(pb, ENUM_NODE_VAR);
}

int MsgTool::getExtraPos(NodeMsg* pb) {
    return NodeUtil::getPos(pb, ENUM_NODE_VAR);
}

void MsgTool::setExtraPos(NodeMsg* pb, int pos) {
    NodeUtil::setPos(pb, ENUM_NODE_VAR, pos);
}

void MsgTool::skipExtraPos(NodeMsg* pb, int pos) {
    NodeUtil::skipPos(pb, ENUM_NODE_VAR, pos);
}

int MsgTool::getLeft(NodeMsg* pb) {
    return NodeUtil::getLeft(pb, ENUM_NODE_INFR);
}

int MsgTool::getExtraLeft(NodeMsg* pb) {
    return NodeUtil::getLeft(pb, ENUM_NODE_VAR);
}

NodeMsg* MsgTool::allocUdpMsg(int size) {
    NodeMsg* msg = NULL;

    msg = allocMsg(size, sizeof(SockAddr));
    return msg;
}

NodeMsg* MsgTool::refUdpMsg(NodeMsg* msg) {
    return refNodeMsg(msg, sizeof(SockAddr));
}

int MsgTool::setUdpAddr(NodeMsg* msg, const SockAddr& addr) {
    SockAddr* preh = NULL;
    
    preh = (SockAddr*)getPreNode(msg, sizeof(SockAddr));
    if (NULL != preh) {
        SockTool::setAddr(*preh, addr);

        return 0;
    } else {
        return -1;
    }
}

const SockAddr* MsgTool::getUdpAddr(NodeMsg* msg) {
    SockAddr* preh = NULL;
    
    preh = (SockAddr*)getPreNode(msg, sizeof(SockAddr));
    if (NULL != preh) {
        return preh;
    } else {
        return NULL;
    }
}


