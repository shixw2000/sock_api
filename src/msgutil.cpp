#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"msgutil.h"
#include"msgtool.h"


struct GenCmd_t {
    int m_size;
    int m_ref_cnt;
    char m_body[0];
} __attribute__((aligned(8)));

struct NodeCmd {
    LList m_list;
    GenCmd_t* m_head;
} __attribute__((aligned(8)));


struct NodeMsg;
struct GenMsg_t {
    NodeMsg* m_origin;
    int m_capacity; 
    int m_ref_cnt;
    char m_body[0];
} __attribute__((aligned(8)));

struct NodeMsg {
    LList m_list;
    GenMsg_t* m_head;
    int m_size;
    int m_pos;
    int m_preLen; /* after may have extends datas */
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
    char* psz = NULL;
    NodeMsg* pb = NULL;
    GenMsg_t* ph = NULL;
    int total = _alignment(size) + DEF_MSG_OFFSET; 

    psz = (char*)malloc(total); 
    memset(psz, 0, total);

    pb = reinterpret_cast<NodeMsg*>(psz);
    ph = reinterpret_cast<GenMsg_t*>(psz + sizeof(NodeMsg));

    initList(&pb->m_list); 
    pb->m_head = ph;
    pb->m_size = size;
    pb->m_pos = 0;
    pb->m_preLen = 0;
    
    ph->m_origin = pb;
    ph->m_capacity = size;
    ph->m_ref_cnt = 1;

    return pb;
}

NodeMsg* MsgUtil::_creatPreNodeMsg(int preLen, int size) {
    char* psz = NULL;
    NodeMsg* pb = NULL;
    GenMsg_t* ph = NULL;
    int total = 0;
    int resLen = 0;

    resLen = _alignment(preLen);
    total = resLen + _alignment(size) + DEF_MSG_OFFSET; 
    psz = (char*)malloc(total); 
    memset(psz, 0, total);

    pb = reinterpret_cast<NodeMsg*>(psz);
    ph = reinterpret_cast<GenMsg_t*>(psz 
        + (int)sizeof(NodeMsg) + resLen); 

    initList(&pb->m_list); 
    pb->m_head = ph;
    pb->m_size = size;
    pb->m_pos = 0;
    pb->m_preLen = preLen;
    
    ph->m_origin = pb;
    ph->m_capacity = size;
    ph->m_ref_cnt = 1;

    return pb;
}

void MsgUtil::freeNodeMsg(NodeMsg* pb) {
    int ref = 0;
    NodeMsg* origin = NULL;
    GenMsg_t* ph = pb->m_head; 

    origin = ph->m_origin;
    
    ref = ATOMIC_DEC_FETCH(&ph->m_ref_cnt);
    if (origin == pb) {
        /* the real node */
        if (0 >= ref) {
            free(pb);
        }
    } else {
        /* the referenced node */
        free(pb);

        if (0 >= ref) {
            free(origin);
        }
    }
}

NodeMsg* MsgUtil::refNodeMsg(NodeMsg* ref) {
    GenMsg_t* ph = ref->m_head;
    char* psz = NULL;
    NodeMsg* pb = NULL;
    int total = sizeof(NodeMsg);

    psz = (char*)malloc(total);
    memset(psz, 0, total);

    pb = reinterpret_cast<NodeMsg*>(psz);
    initList(&pb->m_list); 
    pb->m_size = ref->m_size;
    pb->m_pos = 0;
    pb->m_preLen = 0;

    pb->m_head = ph;
    ATOMIC_INC_FETCH(&ph->m_ref_cnt);

    return pb;
}

NodeMsg* MsgUtil::_refPreNodeMsg(int preLen,
    NodeMsg* ref) {
    GenMsg_t* ph = ref->m_head;
    char* psz = NULL;
    NodeMsg* pb = NULL;
    int resLen = 0;
    int total = 0;

    resLen = _alignment(preLen);
    total = (int)sizeof(NodeMsg) + resLen;

    psz = (char*)malloc(total);
    memset(psz, 0, total);

    pb = reinterpret_cast<NodeMsg*>(psz);
    initList(&pb->m_list); 
    pb->m_size = ref->m_size;
    pb->m_pos = 0;
    pb->m_preLen = preLen;
    
    pb->m_head = ph;
    ATOMIC_INC_FETCH(&ph->m_ref_cnt);
    
    return pb; 
}

char* MsgUtil::_getPreHead(int preLen,
    NodeMsg* pb) {
    char* psz = NULL;
    
    if (0 < preLen && preLen == pb->m_preLen) {
        psz = reinterpret_cast<char*>(pb);
        psz += sizeof(NodeMsg);
        
        return psz;
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

char* MsgUtil::getMsg(NodeMsg* pb) {
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


NodeMsg* MsgTool::allocMsg(int total) {
    return MsgUtil::creatNodeMsg(total);;
}

void MsgTool::freeMsg(NodeMsg* pMsg) {
    MsgUtil::freeNodeMsg(pMsg);
}

char* MsgTool::getMsg(NodeMsg* pb) {
    return MsgUtil::getMsg(pb);
}

bool MsgTool::completedMsg(NodeMsg* pb) {
    return MsgUtil::completedMsg(pb);
}

void MsgTool::setMsgSize(NodeMsg* pb, int size) {
    MsgUtil::setMsgSize(pb, size);
}

int MsgTool::getMsgSize(const NodeMsg* pb) {
    return MsgUtil::getMsgSize(pb);
}

int MsgTool::getMsgPos(const NodeMsg* pb) {
    return MsgUtil::getMsgPos(pb);
}

void MsgTool::setMsgPos(NodeMsg* pb, int pos) {
    MsgUtil::setMsgPos(pb, pos);
}

void MsgTool::skipMsgPos(NodeMsg* pb, int pos) {
    MsgUtil::skipMsgPos(pb, pos);
}

NodeMsg* MsgTool::refNodeMsg(NodeMsg* pb) {
    return MsgUtil::refNodeMsg(pb);
}

char* MsgTool::_getPreHead(int preLen, 
    NodeMsg* pb) {
    return MsgUtil::_getPreHead(preLen, pb);
}

NodeMsg* MsgTool::_creatPreMsg(
    int preLen, int size) {
    return MsgUtil::_creatPreNodeMsg(preLen, size);
}

NodeMsg* MsgTool::_refPreMsg(
    int preLen, NodeMsg* ref) {
    return MsgUtil::_refPreNodeMsg(preLen, ref);
}

