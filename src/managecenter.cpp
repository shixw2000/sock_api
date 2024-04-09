#include<cstring>
#include<cstdlib>
#include"managecenter.h"
#include"msgutil.h"
#include"lock.h"
#include"ticktimer.h"
#include"misc.h"
#include"isockapi.h"


struct GenData { 
    LList m_rd_node;
    LList m_wr_node;
    LList m_deal_node;

    LList m_rd_timeout_node;
    LList m_wr_timeout_node;

    LList m_wr_msg_que;
    LList m_wr_msg_que_priv;
    LList m_deal_msg_que;

    TimerObj m_rd_flowctl;
    TimerObj m_wr_flowctl;
    
    long m_extra;
    long m_data2;
    
    int m_fd; 

    int m_rd_stat;
    int m_rd_index;
    int m_rd_cb; 
    
    int m_wr_stat;
    int m_wr_cb;
    int m_wr_conn;

    int m_deal_stat;
    int m_deal_cb; 

    unsigned m_closing; 

    unsigned m_rd_timeout_thresh;
    unsigned m_rd_expire;
    unsigned m_wr_timeout_thresh;
    unsigned m_wr_expire;
    
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    unsigned m_rd_bytes[DEF_NUM_PER_SEC];
    unsigned m_wr_bytes[DEF_NUM_PER_SEC];
    unsigned m_rd_last_time;
    unsigned m_wr_last_time; 
    
    SockBuffer m_rd_buf;
    int m_port;
    char m_szIP[DEF_IP_SIZE];
};


ManageCenter::ManageCenter() : m_cap(MAX_FD_NUM) {
    initQue(&m_pool);
    m_lock = NULL;
    m_cache = NULL;
    m_datas = NULL;
    m_proto = NULL;

    m_conn_timeout = 3;
    m_rd_timeout = 60;
    m_wr_timeout = 60;
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

        initList(&obj->m_wr_msg_que);
        initList(&obj->m_deal_msg_que);

        initList(&obj->m_wr_msg_que_priv);
    }
    
    return obj;
}

void ManageCenter::_releaseData(GenData* ele) {
    if (NULL != ele) { 
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

bool ManageCenter::isClosed(GenData* data) const {
    return 0 < data->m_closing;
}

bool ManageCenter::markClosed(GenData* data) {
    return 0 == data->m_closing++;
}

void ManageCenter::setData(GenData* data, long extra, long data2) {
    data->m_extra = extra; 
    data->m_data2 = data2;
}

void ManageCenter::refData(GenData* dstData, GenData* srcData) {
    dstData->m_extra = srcData->m_extra; 
    dstData->m_data2 = srcData->m_data2;
    dstData->m_rd_thresh = srcData->m_rd_thresh; 
    dstData->m_wr_thresh = srcData->m_wr_thresh;
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

int ManageCenter::getRdIndex(GenData* ele) const {
    return ele->m_rd_index;
}

void ManageCenter::setRdIndex(GenData* ele, int index) {
    ele->m_rd_index = index;
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

int ManageCenter::getCb(EnumDir enDir, GenData* data) const {
    int cb = 0;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        cb = data->m_rd_cb;
        break;

    case ENUM_DIR_SENDER:
        cb = data->m_wr_cb;
        break;

    case ENUM_DIR_DEALER:
        cb = data->m_deal_cb;
        break;

    default:
        break;
    }

    return cb;
}

void ManageCenter::setConnRes(GenData* data, int res) {
    data->m_wr_conn = res;
}

void ManageCenter::addNode(EnumDir enDir, 
    LList* root, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        add(root, &data->m_rd_node);
        break;

    case ENUM_DIR_SENDER:
        add(root, &data->m_wr_node);
        break;

    case ENUM_DIR_DEALER:
        add(root, &data->m_deal_node);
        break;

    default:
        break;
    }
} 

GenData* ManageCenter::fromNode(EnumDir enDir, LList* node) {
    GenData* data = NULL;

    switch (enDir) {
    case ENUM_DIR_RECVER:
        data = CONTAINER(node, GenData, m_rd_node);
        break;

    case ENUM_DIR_SENDER:
        data = CONTAINER(node, GenData, m_wr_node);
        break;

    case ENUM_DIR_DEALER:
        data = CONTAINER(node, GenData, m_deal_node);
        break;
        
    default:
        break;
    }

    return data;
}

void ManageCenter::delNode(EnumDir enDir, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        del(&data->m_rd_node);
        break;

    case ENUM_DIR_SENDER:
        del(&data->m_wr_node);
        break;

    case ENUM_DIR_DEALER:
        del(&data->m_deal_node);
        break;

    default:
        break;
    }
} 

void ManageCenter::addTimeout(EnumDir enDir, 
    LList* root, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        add(root, &data->m_rd_timeout_node);
        break;

    case ENUM_DIR_SENDER:
        add(root, &data->m_wr_timeout_node);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    }
} 

GenData* ManageCenter::fromTimeout(EnumDir enDir, LList* node) {
    GenData* data = NULL;

    switch (enDir) {
    case ENUM_DIR_RECVER:
        data = CONTAINER(node, GenData, m_rd_timeout_node);
        break;

    case ENUM_DIR_SENDER:
        data = CONTAINER(node, GenData, m_wr_timeout_node);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    }

    return data;
}

void ManageCenter::delTimeout(EnumDir enDir, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        del(&data->m_rd_timeout_node);
        break;

    case ENUM_DIR_SENDER:
        del(&data->m_wr_timeout_node);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    }
} 

void ManageCenter::flowctl(EnumDir enDir, 
    TickTimer* timer, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        timer->schedule(&data->m_rd_flowctl, DEF_FLOWCTL_TICK_NUM);
        break;

    case ENUM_DIR_SENDER:
        timer->schedule(&data->m_wr_flowctl, DEF_FLOWCTL_TICK_NUM);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    }
} 

void ManageCenter::unflowctl(EnumDir enDir, GenData* data) {
    switch (enDir) {
    case ENUM_DIR_RECVER:
        TickTimer::delTimer(&data->m_rd_flowctl);
        break;

    case ENUM_DIR_SENDER:
        TickTimer::delTimer(&data->m_wr_flowctl);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    } 
}

void ManageCenter::setFlowctl(EnumDir enDir,
    GenData* data, TFunc func, long ptr) {
        
    switch (enDir) {
    case ENUM_DIR_RECVER:
        TickTimer::setTimerCb(&data->m_rd_flowctl, 
            func, ptr, (long)data);
        break;

    case ENUM_DIR_SENDER:
        TickTimer::setTimerCb(&data->m_wr_flowctl, 
            func, ptr, (long)data);
        break;

    case ENUM_DIR_DEALER:
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
    data->m_rd_thresh = rd_thresh * KILO;
    data->m_wr_thresh = wr_thresh * KILO;
}

void ManageCenter::setConnTimeout(GenData* data) {
    data->m_wr_timeout_thresh = m_conn_timeout;
}

void ManageCenter::setIdleTimeout(GenData* data) {
    data->m_rd_timeout_thresh = m_rd_timeout;
    data->m_wr_timeout_thresh = m_wr_timeout;
}

void ManageCenter::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    m_rd_timeout = rd_timeout * DEF_NUM_PER_SEC;
    m_wr_timeout = wr_timeout * DEF_NUM_PER_SEC;
}

void ManageCenter::setAddr(GenData* data, 
    const char szIP[], int port) {
    data->m_port = port;
    strncpy(data->m_szIP, szIP, sizeof(data->m_szIP)-1);
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

        LOG_INFO("fd=%d| addr=%s:%d| cnt=%u| msg=closed|", 
            fd, data->m_szIP, data->m_port, 
            data->m_closing);

        releaseRd(data);
        releaseWr(data);
        releaseDeal(data);

        initList(&data->m_rd_node);
        initList(&data->m_wr_node);
        initList(&data->m_deal_node);

        initList(&data->m_rd_timeout_node);
        initList(&data->m_wr_timeout_node);

        initList(&data->m_wr_msg_que);
        initList(&data->m_wr_msg_que_priv);
        initList(&data->m_deal_msg_que); 
        
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

int ManageCenter::recvMsg(GenData* data, int max) {
    SockBuffer* buffer = &data->m_rd_buf;
    int fd = getFd(data);
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

void ManageCenter::releaseRd(GenData* data) {
    SockBuffer* buffer = &data->m_rd_buf;
    
    if (NULL != buffer->m_msg) {
        MsgUtil::freeNodeMsg(buffer->m_msg);
        buffer->m_msg = NULL;
    }
}

void ManageCenter::releaseWr(GenData* data) {
    LList runlist = INIT_LIST(runlist);
    
    if (!isEmpty(&data->m_wr_msg_que)) {
        append(&runlist, &data->m_wr_msg_que);
    }

    if (!isEmpty(&data->m_wr_msg_que_priv)) {
        append(&runlist, &data->m_wr_msg_que_priv);
    }

    releaseMsgQue(&runlist);
}

void ManageCenter::releaseDeal(GenData* data) {
    LList runlist = INIT_LIST(runlist); 
    
    if (!isEmpty(&data->m_deal_msg_que)) {
        append(&runlist, &data->m_deal_msg_que);
        
        releaseMsgQue(&runlist);
    }
}

void ManageCenter::releaseMsgQue(LList* list) {
    NodeMsg* base = NULL;
    LList* node = NULL;
    LList* n = NULL; 
    
    if (!isEmpty(list)) {
        for_each_list_safe(node, n, list) {
            base = MsgUtil::toNodeMsg(node);
            del(node);
            MsgUtil::freeNodeMsg(base);
        }
    }
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

int ManageCenter::addMsg(EnumDir enDir,
    GenData* data, NodeMsg* pMsg) {
    int ret = 0;
    
    switch (enDir) { 
    case ENUM_DIR_SENDER:
        MsgUtil::addNodeMsg(&data->m_wr_msg_que, pMsg);
        break;

    case ENUM_DIR_DEALER:
        MsgUtil::addNodeMsg(&data->m_deal_msg_que, pMsg);
        break;
        
    default:
        ret = -1;
        break;
    }

    return ret;
}

void ManageCenter::appendQue(EnumDir enDir, LList* to, GenData* data) {
    switch (enDir) { 
    case ENUM_DIR_SENDER:
        append(to, &data->m_wr_msg_que);
        break;

    case ENUM_DIR_DEALER:
        append(to, &data->m_deal_msg_que);
        break;
        
    default:
        break;
    }
}

LList* ManageCenter::getWrPrivQue(GenData* data) const {
    return &data->m_wr_msg_que_priv;
}

int ManageCenter::onNewSock(GenData* data, int newFd) {
    ISockSvr* svr = (ISockSvr*)data->m_extra;
    long data2 = data->m_data2;
    int listenFd = data->m_fd;
    int ret = 0;

    ret = svr->onNewSock(data2, listenFd, newFd);
    return ret;
}

int ManageCenter::onConnect(GenData* data) {
    ISockCli* cli = (ISockCli*)data->m_extra; 
    long data2 = data->m_data2;
    int fd = data->m_fd;
    int ret = 0;

    if (0 == data->m_wr_conn) {
        ret = cli->onConnOK(data2, fd);
    } else {
        cli->onConnFail(data2, fd);
        ret = data->m_wr_conn;
    }

    return ret;
}

int ManageCenter::onProcess(GenData* data, NodeMsg* msg) {
    ISockBase* base = (ISockBase*)data->m_extra;
    long data2 = data->m_data2;
    int fd = data->m_fd;
    int ret = 0;

    ret = base->process(data2, fd, msg);
    return ret;
}

void ManageCenter::onClose(GenData* data) {
    ISockBase* base = (ISockBase*)data->m_extra;
    long data2 = data->m_data2;
    int fd = data->m_fd;

    base->onClose(data2, fd);
}

