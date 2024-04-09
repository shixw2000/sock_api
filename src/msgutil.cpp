#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"msgutil.h"


struct GenCmd_t {
    int m_size;
    int m_ref_cnt;
    char m_body[0];
} __attribute__((aligned(8)));

struct NodeCmd {
    LList m_list;
    GenCmd_t* m_head;
} __attribute__((aligned(8)));

struct GenMsg_t {
    int m_capacity; 
    int m_ref_cnt;
    char m_body[0];
} __attribute__((aligned(8)));

struct NodeMsg {
    LList m_list;
    GenMsg_t* m_head;
    int m_size;
    int m_pos;
} __attribute__((aligned(8)));

static const int DEF_CMD_OFFSET = sizeof(GenCmd_t)
    + sizeof(NodeCmd);

static const int DEF_MSG_OFFSET = sizeof(GenMsg_t)
    + sizeof(NodeMsg);

int MsgUtil::_alignment(int size) {
    static const int ALIGN_MIN = 0x7;
    static const int ALIGN_MASK = ~ALIGN_MIN;

    return ((size + ALIGN_MIN) & ALIGN_MASK);
}

NodeCmd* MsgUtil::creatNodeCmd(int size) {
    NodeCmd* pb = NULL;
    GenCmd_t* ph = NULL;
    int total = _alignment(size) + DEF_CMD_OFFSET; 

    pb = (NodeCmd*)std::malloc(total); 
    ph = reinterpret_cast<GenCmd_t*>(pb + 1); 
    
    std::memset(pb, 0, total);

    initList(&pb->m_list); 
    pb->m_head = ph;
    
    ph->m_size = size;
    ph->m_ref_cnt = 1;

    return pb;
}

void MsgUtil::freeNodeCmd(NodeCmd* pb) {
    int ref = 0;
    char* psz = NULL;
    NodeCmd* base = NULL;
    GenCmd_t* ph = pb->m_head; 

    psz = reinterpret_cast<char*>(ph);
    base = reinterpret_cast<NodeCmd*>(psz - sizeof(NodeCmd));
    
    ref = ATOMIC_DEC_FETCH(&ph->m_ref_cnt);
    if (base == pb) {
        /* the real node */
        if (0 >= ref) {
            free(base);
        }
    } else {
        /* the referenced node */
        free(pb);

        if (0 >= ref) {
            free(base);
        }
    }
}

NodeCmd* MsgUtil::refNodeCmd(NodeCmd* pb) {
    int total = sizeof(NodeCmd);
    NodeCmd* base = NULL;
    GenCmd_t* ph = pb->m_head;

    base = (NodeCmd*)std::malloc(total); 
    if (NULL != base) {
        ATOMIC_INC_FETCH(&ph->m_ref_cnt);

        std::memset(base, 0, total);
        initList(&base->m_list); 
        base->m_head = ph;
        
        return base;
    } else {
        return NULL;
    }
}

void MsgUtil::addNodeCmd(LList* root, NodeCmd* pb) { 
    add(root, &pb->m_list);
}

const NodeCmd* MsgUtil::toNodeCmd(const LList* node) {
    return reinterpret_cast<const NodeCmd*>(node);
}

NodeCmd* MsgUtil::toNodeCmd(LList* node) {
    return reinterpret_cast<NodeCmd*>(node);
}

CmdHead_t* MsgUtil::getCmd(const NodeCmd* pb) {
    GenCmd_t* data = pb->m_head;

    return reinterpret_cast<CmdHead_t*>(data->m_body);
} 


bool MsgUtil::completedMsg(NodeMsg* pb) {
    if (NULL != pb) {
        return pb->m_size == pb->m_pos;
    } else {
        return false;
    }
}

NodeMsg* MsgUtil::creatNodeMsg(int size) {
    NodeMsg* pb = NULL;
    GenMsg_t* ph = NULL;
    int total = _alignment(size) + DEF_MSG_OFFSET; 

    pb = (NodeMsg*)std::malloc(total); 
    ph = reinterpret_cast<GenMsg_t*>(pb + 1); 
    
    std::memset(pb, 0, total);

    initList(&pb->m_list); 
    pb->m_head = ph;
    pb->m_size = size;
    pb->m_pos = 0;
    
    ph->m_capacity = size;
    ph->m_ref_cnt = 1;

    return pb;
}

void MsgUtil::freeNodeMsg(NodeMsg* pb) {
    int ref = 0;
    char* psz = NULL;
    NodeMsg* base = NULL;
    GenMsg_t* ph = pb->m_head; 

    psz = reinterpret_cast<char*>(ph);
    base = reinterpret_cast<NodeMsg*>(psz - sizeof(NodeMsg));
    
    ref = ATOMIC_DEC_FETCH(&ph->m_ref_cnt);
    if (base == pb) {
        /* the real node */
        if (0 >= ref) {
            free(base);
        }
    } else {
        /* the referenced node */
        free(pb);

        if (0 >= ref) {
            free(base);
        }
    }
}

NodeMsg* MsgUtil::refNodeMsg(NodeMsg* pb) {
    int total = sizeof(NodeMsg);
    NodeMsg* base = NULL;
    GenMsg_t* ph = pb->m_head;

    base = (NodeMsg*)std::malloc(total); 
    if (NULL != base) {
        ATOMIC_INC_FETCH(&ph->m_ref_cnt);

        std::memset(base, 0, total);
        initList(&base->m_list); 
        base->m_size = pb->m_size;
        base->m_pos = 0;
        base->m_head = ph;
        
        return base;
    } else {
        return NULL;
    }
}

void MsgUtil::addNodeMsg(LList* root, NodeMsg* pb) { 
    add(root, &pb->m_list);
}

NodeMsg* MsgUtil::toNodeMsg(LList* node) {
    return reinterpret_cast<NodeMsg*>(node);
}

const NodeMsg* MsgUtil::toNodeMsg(const LList* node) {
    return reinterpret_cast<const NodeMsg*>(node);
}

char* MsgUtil::getMsg(const NodeMsg* pb) {
    return pb->m_head->m_body;
}

void MsgUtil::setMsgSize(NodeMsg* pb, int size) {
    pb->m_size = size;
}

int MsgUtil::getMsgSize(const NodeMsg* pb) {
    return pb->m_size;
}

int MsgUtil::getMsgPos(const NodeMsg* pb) {
    return pb->m_pos;
}

void MsgUtil::setMsgPos(NodeMsg* pb, int pos) {
    pb->m_pos = pos;
}

void MsgUtil::skipMsgPos(NodeMsg* pb, int pos) {
    pb->m_pos += pos;
}

