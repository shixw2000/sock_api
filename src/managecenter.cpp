#include<cstring>
#include<cstdlib>
#include"managecenter.h"
#include"msgutil.h"
#include"lock.h"
#include"isockmsg.h"
#include"ticktimer.h"
#include"misc.h"


ManageCenter::ManageCenter() : m_cap(MAX_FD_NUM) {
    initQue(&m_pool);
    m_lock = NULL;
    m_cache = NULL;
    m_datas = NULL;
    m_proto = NULL;

    m_rd_timeout = 0;
    m_wr_timeout = 0;
}

ManageCenter::~ManageCenter() {
}

int ManageCenter::init() {
    int ret = 0; 
    
    m_lock = new SpinLock;
    
    m_cache = (GenData*)calloc(m_cap, sizeof(GenData));
    m_datas = (GenData**)calloc(m_cap, sizeof(GenData*));
    
    creatQue(&m_pool, m_cap);
    
    for (int i=0; i<m_cap; ++i) {
        push(&m_pool, &m_cache[i]);
    }

    return ret;
}

void ManageCenter::finish() {
    finishQue(&m_pool);

    if (NULL != m_proto) {
        
        //delete m_proto;
        m_proto = NULL;
    }
    
    if (NULL != m_datas) {
        free(m_datas);

        m_datas = NULL;
    }

    if (NULL != m_cache) {
        free(m_cache);

        m_cache = NULL;
    } 

    if (NULL != m_lock) {
        delete m_lock;
        m_lock = NULL;
    }
}

void ManageCenter::lock() {
    m_lock->lock();
}

void ManageCenter::unlock() {
    m_lock->unlock();
}

GenData* ManageCenter::_allocData() {
    bool bOk = NULL;
    void* ele = NULL;
    GenData* obj = NULL;
    
    lock();
    bOk = pop(&m_pool, &ele);
    unlock(); 

    if (bOk) {
        obj = reinterpret_cast<GenData*>(ele); 
        memset(obj, 0, sizeof(GenData));

        initList(&obj->m_rd_node);
        initList(&obj->m_wr_node);
        initList(&obj->m_deal_node);

        initList(&obj->m_rd_timeout_node);
        initList(&obj->m_wr_timeout_node);

        initList(&obj->m_rd_msg_que);
        initList(&obj->m_wr_msg_que);
        initList(&obj->m_deal_msg_que);
    }
    
    return obj;
}

void ManageCenter::_releaseData(GenData* ele) {
    if (NULL != ele) { 
        if (NULL != ele->m_wr_cur_msg) {
            MsgUtil::freeNodeMsg(ele->m_wr_cur_msg);
            ele->m_wr_cur_msg = NULL;
        }

        MsgUtil::freeBuffer(&ele->m_rd_buf);
        
        lock();
        push(&m_pool, ele);
        unlock();
    }
}

GenData* ManageCenter::find(int fd) const {
    if (exists(fd)) {
        return m_datas[fd];
    } else {
        return NULL;
    }
}

bool ManageCenter::exists(int fd) const {
    return  NULL != m_datas[fd];
} 

void ManageCenter::setData(GenData* data, long extra, long data2) {
    data->m_extra = extra; 
    data->m_data2 = data2;
}

void ManageCenter::setStat(EnumDir enDir, GenData* data, int stat) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        data->m_rd_stat = stat;
        break;

    case ENUM_DIR_SENDER:
        data->m_wr_stat = stat;
        break;

    case ENUM_DIR_DEALER:
        data->m_deal_stat = stat;
        break;

    default:
        break;
    }
}

int ManageCenter::getFd(GenData* ele) const {
    return ele->m_fd;
}

int ManageCenter::getStat(EnumDir enDir, GenData* data) const {
    int stat = 0;

    switch (enDir) {
    case ENUM_DIR_RECVER:
        stat = data->m_rd_stat;
        break;

    case ENUM_DIR_SENDER:
        stat = data->m_wr_stat;
        break;

    case ENUM_DIR_DEALER:
        stat = data->m_deal_stat;
        break;

    default:
        break;
    }

    return stat;
}

void ManageCenter::setCb(EnumDir enDir,
    GenData* data, int cb) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        data->m_rd_cb = cb;
        break;

    case ENUM_DIR_SENDER:
        data->m_wr_cb = cb;
        break;

    case ENUM_DIR_DEALER:
        data->m_deal_cb = cb;
        break;

    default:
        break;
    }
} 

void ManageCenter::recordBytes(EnumDir enDir, 
    GenData* data, unsigned now, unsigned total) {
    unsigned (*pa)[DEF_NUM_PER_SEC] = NULL;
    unsigned* plast = 0;
    unsigned index = 0;

    switch (enDir) {
    case ENUM_DIR_RECVER:
        pa = &data->m_rd_bytes;
        plast = &data->m_rd_last_time;
        break;

    case ENUM_DIR_SENDER:
        pa = &data->m_wr_bytes;
        plast = &data->m_wr_last_time;
        break;

    default:
        return;
        break;
    }

    index = now & MASK_PER_SEC;
    (*pa)[index] += total;
    *plast = now;
}

void ManageCenter::clearBytes(EnumDir enDir, 
    GenData* data, unsigned now) {
    unsigned (*pa)[DEF_NUM_PER_SEC] = NULL;
    unsigned last = 0;
    unsigned diff = 0;
    unsigned index = 0;

    switch (enDir) {
    case ENUM_DIR_RECVER:
        pa = &data->m_rd_bytes;
        last = data->m_rd_last_time;
        break;

    case ENUM_DIR_SENDER:
        pa = &data->m_wr_bytes;
        last = data->m_wr_last_time;
        break;

    default:
        return;
        break;
    }
    
    diff = (now - last); 
    if (DEF_NUM_PER_SEC < diff) {
        diff = DEF_NUM_PER_SEC;
    } 

    /* clear unused */
    while (0 < diff) {
        index = (++last) & MASK_PER_SEC; 
        (*pa)[index] = 0;
        --diff;
    } 
}

void ManageCenter::calcBytes(EnumDir enDir, 
    GenData* data, unsigned now, 
    unsigned& total, unsigned& total2, 
    unsigned& thresh) {
    unsigned (*pa)[DEF_NUM_PER_SEC] = NULL;
    unsigned index = 0;
    unsigned half = 0;
    unsigned v = 0;

    total = total2 = thresh = 0;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        thresh = data->m_rd_thresh;
        pa = &data->m_rd_bytes;
        break;

    case ENUM_DIR_SENDER:
        thresh = data->m_wr_thresh;
        pa = &data->m_wr_bytes;
        break;

    default: 
        return;
        break;
    }

    half = thresh >> 1;
    index = now; 
    for (int i=0; i<2; ++i) {
        index = index & MASK_PER_SEC;
        v = (*pa)[index] <= half ? (*pa)[index] : half;
        total2 += v;

        --index;
    }

    total = total2;
    for (int i=0; i<2; ++i) {
        index = index & MASK_PER_SEC;
        v = (*pa)[index] <= half ? (*pa)[index] : half;
        total += v;

        --index;
    }
}

unsigned ManageCenter::calcThresh(EnumDir enDir, 
    GenData* data, unsigned now) {
    unsigned total = 0;
    unsigned total2 = 0;
    unsigned thresh = 0;
    unsigned half = 0;
    unsigned quarter = 0;
    unsigned max = 0;

    calcBytes(enDir, data, now, total, total2, thresh);
    half = thresh >> 1;
    quarter = half >> 1;

    if (total >= thresh || total2 >= half) {
        /* flow control */
        max = 0;
    } else if (total < quarter) {
        /* too low to add more */
        max = thresh - total; 
    } else {
        max = half - total2;
    } 

    return max;
}

unsigned ManageCenter::getRdThresh(GenData* data) const {
    return data->m_rd_thresh;
}

unsigned ManageCenter::getWrThresh(GenData* data) const {
    return data->m_wr_thresh;
}

void ManageCenter::setSpeed(GenData* data, 
    unsigned rd_thresh, unsigned wr_thresh) {
    data->m_rd_thresh = rd_thresh;
    data->m_wr_thresh = wr_thresh;
    
    data->m_rd_timeout_thresh = m_rd_timeout;
    data->m_wr_timeout_thresh = m_wr_timeout;
}

void ManageCenter::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    m_rd_timeout = rd_timeout;
    m_wr_timeout = wr_timeout;
}

void ManageCenter::setAddr(GenData* data, 
    const char szIP[], int port) {
    data->m_port = port;
    strncpy(data->m_szIP, szIP, sizeof(data->m_szIP)-1);
}

bool ManageCenter::setClose(GenData* data) {
    unsigned n = 0;
    
    lock();
    n = data->m_closing++;
    unlock();

    return 0 == n;
}

GenData* ManageCenter::reg(int fd) {
    GenData* data = NULL;

    if (0 <= fd && fd < m_cap && !exists(fd)) {
        data = _allocData();
        if (NULL != data) { 
            data->m_fd = fd;
            m_datas[fd] = data;
            
            return data;
        } 
    }

    return NULL;
} 

void ManageCenter::unreg(int fd) {
    if (exists(fd)) {
        GenData* data = find(fd);
        
        data->m_fd = -100000;
        m_datas[fd] = NULL;
        _releaseData(data);
    }
}

NodeCmd* ManageCenter::creatCmdComm(EnumSockCmd cmd, int fd) {
    NodeCmd* pCmd = NULL;
    CmdComm* body = NULL;
    
    pCmd = MsgUtil::creatCmd<CmdComm>(cmd);
    body = MsgUtil::getCmdBody<CmdComm>(pCmd);
    body->m_fd = fd;

    return pCmd;
}

NodeCmd* ManageCenter::creatCmdSchedTask(unsigned delay,
    TFunc func, long data, long data2) {
    NodeCmd* pCmd = NULL;
    CmdSchedTask* body = NULL;
        
    pCmd = MsgUtil::creatCmd<CmdSchedTask>(ENUM_CMD_SCHED_TASK);
    body = MsgUtil::getCmdBody<CmdSchedTask>(pCmd);

    body->func = func;
    body->m_data = data;
    body->m_data2 = data2;
    body->m_delay = delay;

    return pCmd;
} 

bool ManageCenter::updateExpire(EnumDir enDir, 
    GenData* data, unsigned now) {
    const unsigned* pt = NULL;
    unsigned* pe = NULL;
    bool action = false;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        pt = &data->m_rd_timeout_thresh;
        pe = &data->m_rd_expire;
        break;

    case ENUM_DIR_SENDER:
        pt = &data->m_wr_timeout_thresh;
        pe = &data->m_wr_expire;
        break;

    default:
        return false;
        break;
    }

    if (0 < *pt) {
        *pe = now + *pt;
        action = true;
    } 

    return action;
}

bool ManageCenter::chkExpire(EnumDir enDir, 
    const GenData* data, unsigned now) {
    const unsigned* pt = NULL;
    const unsigned* pe = NULL;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        pt = &data->m_rd_timeout_thresh;
        pe = &data->m_rd_expire;
        break;

    case ENUM_DIR_SENDER:
        pt = &data->m_wr_timeout_thresh;
        pe = &data->m_wr_expire;
        break;

    default:
        return false;
        break;
    }

    return (*pe - now) > *pt;
}

void ManageCenter::setProto(SockProto* proto) {
    m_proto = proto;
}

int ManageCenter::recvMsg(int fd, SockBuffer* buffer, int max) {
    int ret = 0;
    bool bOk = false;

    if (0 == max || max > MAX_BUFF_SIZE) {
        max = MAX_BUFF_SIZE;
    }
    
    ret = SockTool::recvTcp(fd, m_buf, max);
    if (0 < ret) {
        bOk = m_proto->parseData(fd, buffer, m_buf, ret);
        if (!bOk) {
            ret = -1;
        }
    }
    
    return ret;
}

int ManageCenter::writeMsg(int fd, NodeMsg* pMsg, int max) {
    int ret = 0;
    int size = 0;
    int pos = 0;
    char* psz = NULL;

    size = MsgUtil::getMsgSize(pMsg);
    pos = MsgUtil::getMsgPos(pMsg);
    psz = MsgUtil::getMsg(pMsg);

    psz += pos;
    size -= pos;
    
    if (0 < size) {
        if (0 < max && size > max) {
            size = max;
        }
        
        ret = SockTool::sendTcp(fd, psz, size);
        if (0 < ret) {
            MsgUtil::skipMsgPos(pMsg, ret);
        } 
    }

    return ret;
}

