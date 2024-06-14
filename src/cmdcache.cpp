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
    Buffer* buffer = NULL;
    CmdHead_t* ph = NULL;
    int total = sizeof(CmdHead_t) + size;
    bool bOk = false;

    pb = NodeUtil::creatNode(); 
    if (NULL != pb) {
        buffer = NodeUtil::getBuffer(pb);
        
        bOk = CacheUtil::allocBuffer(buffer, total);
        if (bOk) {
            ph = (CmdHead_t*)CacheUtil::origin(buffer);

            MiscTool::bzero(ph, total);
            ph->m_cmd = cmd;
            ph->m_size = size;
        } else {
            NodeUtil::freeNode(pb);
            pb = NULL;
        } 
    }

    return pb;
}

int CmdUtil::getCmd(NodeMsg* pb) {
    Buffer* buffer = NULL;
    CmdHead_t* ph = NULL;

    buffer = NodeUtil::getBuffer(pb);
    ph = (CmdHead_t*)CacheUtil::origin(buffer);
    
    return ph->m_cmd;
} 

void* CmdUtil::getBody(NodeMsg* pb) {
    Buffer* buffer = NULL;
    CmdHead_t* ph = NULL;

    buffer = NodeUtil::getBuffer(pb);
    ph = (CmdHead_t*)CacheUtil::origin(buffer);
    
    return ph->m_body;
}

NodeMsg* CmdUtil::creatSockExitMsg() {
    NodeMsg* msg = NULL;
    
    msg = NodeUtil::creatNode(); 
    NodeUtil::setNodeType(msg, ENUM_NODE_SOCK_EXIT);
    return msg;
}

bool CmdUtil::isSockExit(NodeMsg* msg) {
    if (ENUM_NODE_SOCK_EXIT !=
        NodeUtil::getNodeType(msg)) {
        return false;
    } else {
        return true;
    }
}

