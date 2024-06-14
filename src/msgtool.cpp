#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"msgtool.h"
#include"nodebase.h"
#include"cache.h"
#include"misc.h"
#include"socktool.h"
 

NodeMsg* MsgTool::allocNode(int prelen) {
    NodeMsg* pb = NULL;

    pb = NodeUtil::creatNode(prelen);
    return pb;
}

NodeMsg* MsgTool::allocMsg(int size, int prelen) {
    NodeMsg* pb = NULL;
    Buffer* buffer = NULL;
    char* psz = NULL;
    bool bOk = false;

    pb = NodeUtil::creatNode(prelen); 
    if (NULL != pb) {
        buffer = NodeUtil::getBuffer(pb);
        
        bOk = CacheUtil::allocBuffer(buffer, size);
        if (bOk) {
            psz = CacheUtil::origin(buffer); 
            MiscTool::bzero(psz, size);
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

void MsgTool::setCb(NodeMsg* pb, PFree pf, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    if (NULL != buffer->m_cache) {
        CacheUtil::setCb(buffer->m_cache, pf);
    }
}

char* MsgTool::getMsg(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    return CacheUtil::origin(buffer);
}

char* MsgTool::getCurr(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    return CacheUtil::curr(buffer);
} 

bool MsgTool::completedMsg(NodeMsg* pb) {
    return NodeUtil::isCompleted(pb);
}

char* MsgTool::getPreNode(NodeMsg* pb, int preLen) {
    return NodeUtil::getPreNode(pb, preLen);
}

void MsgTool::setMsgSize(NodeMsg* pb, int size, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext); 
    CacheUtil::setSize(buffer, size); 
}

int MsgTool::getMsgSize(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    return buffer->m_size;
}

int MsgTool::getMsgPos(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    return buffer->m_pos;
}

void MsgTool::setMsgPos(NodeMsg* pb, int pos, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext); 
    CacheUtil::setPos(buffer, pos); 
}

void MsgTool::skipMsgPos(NodeMsg* pb, int pos, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext); 
    CacheUtil::skipPos(buffer, pos);
} 

Buffer* MsgTool::getBuffer(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    return buffer;
}

void MsgTool::flip(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext);
    CacheUtil::flip(buffer);
}

void MsgTool::setCache(NodeMsg* pb, 
    Cache* cache, int size, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext); 
    CacheUtil::setCache(buffer, cache);
    CacheUtil::setSize(buffer, size); 
} 

int MsgTool::getLeft(NodeMsg* pb, bool ext) {
    Buffer* buffer = NULL;

    buffer = NodeUtil::getBuffer(pb, ext); 
    return CacheUtil::leftLen(buffer);
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


