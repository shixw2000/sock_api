#include<cstring>
#include<cstdlib>
#include"managecenter.h"
#include"msgtool.h"
#include"lock.h"
#include"ticktimer.h"
#include"misc.h"
#include"isockapi.h"
#include"socktool.h"
#include"cmdcache.h"
#include"nodebase.h"
#include"cache.h"

struct TimerEle {
    TimerObj m_timer;
    int m_event;
    unsigned m_timeout_thresh;
    unsigned m_byte_thresh;
    unsigned m_last_time;
    unsigned m_updated_time;
    unsigned m_bytes[DEF_NUM_PER_SEC];
};

struct GenData { 
    LList m_rd_node;
    LList m_wr_node;
    LList m_deal_node;

    LList m_wr_msg_que;
    LList m_wr_msg_que_priv;
    LList m_deal_msg_que;
    LList m_deal_msg_que_priv;

    TimerEle m_timer_ele[ENUM_DIR_END]; 
    
    ISockBase* m_sock;
    long m_extra;
    
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
    int m_port;
    char m_szIP[DEF_IP_SIZE];
};


ManageCenter::ManageCenter(int cap) 
    : m_cap(0 < cap ? cap : MAX_FD_NUM) {
    initQue(&m_pool);
    m_lock = NULL;
    m_cache = NULL;
    m_datas = NULL;

    m_conn_timeout = DEF_CONN_TIMEOUT_TICK;
    m_rd_timeout = DEF_RDWR_TIMEOUT_TICK;
    m_wr_timeout = DEF_RDWR_TIMEOUT_TICK;
}

ManageCenter::~ManageCenter() {
}

int ManageCenter::init() {
    int ret = 0; 
    
    m_lock = new SpinLock;
    
    m_cache = (GenData*)CacheUtil::callocAlign(
        m_cap, sizeof(GenData));
    
    m_datas = (GenData**)CacheUtil::callocAlign(
        m_cap, sizeof(GenData*));

    creatQue(&m_pool, m_cap);
    
    for (int i=0; i<m_cap; ++i) {
        push(&m_pool, &m_cache[i]);
    }

    return ret;
}

void ManageCenter::finish() {
    finishQue(&m_pool);
    
    if (NULL != m_datas) {
        CacheUtil::freeAlign(m_datas);

        m_datas = NULL;
    }

    if (NULL != m_cache) {
        CacheUtil::freeAlign(m_cache);

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

void ManageCenter::_allocData(GenData* obj) { 
    if (NULL != obj) {
        MiscTool::bzero(obj, sizeof(GenData));

        initList(&obj->m_rd_node);
        initList(&obj->m_wr_node);
        initList(&obj->m_deal_node);

        initList(&obj->m_wr_msg_que);
        initList(&obj->m_wr_msg_que_priv);

        initList(&obj->m_deal_msg_que);
        initList(&obj->m_deal_msg_que_priv);
    }
}

void ManageCenter::_releaseData(GenData* ele) {
    if (NULL != ele) { 
        if (!isEmpty(&ele->m_wr_msg_que)) {
            releaseMsgQue(&ele->m_wr_msg_que);
        }

        if (!isEmpty(&ele->m_wr_msg_que_priv)) {
            releaseMsgQue(&ele->m_wr_msg_que_priv);
        }

        if (!isEmpty(&ele->m_deal_msg_que)) {
            releaseMsgQue(&ele->m_deal_msg_que);
        }

        if (!isEmpty(&ele->m_deal_msg_que_priv)) {
            releaseMsgQue(&ele->m_deal_msg_que_priv);
        } 
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
    return  0 < fd && fd < m_cap &&
        NULL != m_datas[fd];
} 

bool ManageCenter::isClosed(GenData* data) const {
    return !!data->m_closing;
}

bool ManageCenter::markClosed(GenData* data) {
    bool close = false;

    lock();
    close = !data->m_closing;
    ++data->m_closing;
    unlock();
    
    return close;
} 

long ManageCenter::getExtra(const GenData* data) {
    return data->m_extra;
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

void ManageCenter::startTimer(EnumDir enDir, 
    TickTimer* timer, GenData* data, int event, 
    unsigned delay) { 
    TimerEle* ele = &data->m_timer_ele[enDir];

    ele->m_event = event;
    if (0 == delay) {
        timer->startTimer(&ele->m_timer, ele->m_timeout_thresh);
    } else {
        timer->startTimer(&ele->m_timer, delay);
    }
} 

void ManageCenter::restartTimer(EnumDir enDir, 
    TickTimer* timer, GenData* data, int event, 
    unsigned delay) { 
    TimerEle* ele = &data->m_timer_ele[enDir];

    ele->m_event = event;
    if (0 == delay) {
        timer->restartTimer(&ele->m_timer, ele->m_timeout_thresh);
    } else {
        timer->restartTimer(&ele->m_timer, delay);
    }
}

void ManageCenter::cancelTimer(EnumDir enDir, 
    TickTimer* timer, GenData* data) {
    TimerEle* ele = &data->m_timer_ele[enDir];

    timer->stopTimer(&ele->m_timer);
}

void ManageCenter::setTimerParam(EnumDir enDir, 
    GenData* data, TimerFunc func, long param1) {
    TimerEle* ele = &data->m_timer_ele[enDir];

    TickTimer::setTimerCb(&ele->m_timer, 
        func, param1, (long)data); 
}

int ManageCenter::getEvent(EnumDir enDir, GenData* data) {
    TimerEle* ele = &data->m_timer_ele[enDir];

    return ele->m_event;
}

void ManageCenter::recordBytes(EnumDir enDir, 
    GenData* data, unsigned now, unsigned total) {
    TimerEle* ele = &data->m_timer_ele[enDir];
    unsigned index = 0;

    index = now & MASK_PER_SEC;
    ele->m_bytes[index] += total;
    ele->m_last_time = now; 
}

void ManageCenter::clearBytes(EnumDir enDir, 
    GenData* data, unsigned now) {
    TimerEle* ele = &data->m_timer_ele[enDir]; 
    unsigned last = ele->m_last_time;
    unsigned index = 0;
    unsigned diff = 0;

    diff = (now - last); 
    if (DEF_NUM_PER_SEC < diff) {
        diff = DEF_NUM_PER_SEC;
    } 

    /* clear unused */
    while (0 < diff) {
        index = (++last) & MASK_PER_SEC; 
        ele->m_bytes[index] = 0;
        --diff;
    } 
}

void ManageCenter::calcBytes(EnumDir enDir, 
    GenData* data, unsigned now, 
    unsigned& total, unsigned& total2, 
    unsigned& thresh) {
    TimerEle* ele = &data->m_timer_ele[enDir]; 
    unsigned index = 0;
    unsigned half = 0;
    unsigned v = 0;

    total = total2 = 0;
    thresh = ele->m_byte_thresh;

    half = thresh >> 1;

    for (int i=0; i<ARR_CNT(ele->m_bytes); ++i) {
        v = ele->m_bytes[i] <= half ?
            ele->m_bytes[i] : half;
        
        total += v;
    }
    
    index = now & MASK_PER_SEC;
    v = ele->m_bytes[index] <= half ?
        ele->m_bytes[index] : half;
    total2 += v;

    index = (now-1) & MASK_PER_SEC;
    v = ele->m_bytes[index] <= half ?
        ele->m_bytes[index] : half;
    total2 += v;
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
    return data->m_timer_ele[ENUM_DIR_RECVER].m_byte_thresh;
}

unsigned ManageCenter::getWrThresh(GenData* data) const {
    return data->m_timer_ele[ENUM_DIR_SENDER].m_byte_thresh;
}

void ManageCenter::setSpeed(GenData* data, 
    unsigned rd_thresh, unsigned wr_thresh) {
    data->m_timer_ele[ENUM_DIR_RECVER].m_byte_thresh = rd_thresh;
    data->m_timer_ele[ENUM_DIR_SENDER].m_byte_thresh = wr_thresh;
}

void ManageCenter::setDefConnTimeout(GenData* data) {
    data->m_timer_ele[ENUM_DIR_SENDER].m_timeout_thresh = m_conn_timeout;
}

void ManageCenter::setDefIdleTimeout(GenData* data) {
    data->m_timer_ele[ENUM_DIR_RECVER].m_timeout_thresh = m_rd_timeout;
    data->m_timer_ele[ENUM_DIR_SENDER].m_timeout_thresh = m_wr_timeout;
}

void ManageCenter::setMaxRdTimeout(
    GenData* data, unsigned timeout) {
    data->m_timer_ele[ENUM_DIR_RECVER].m_timeout_thresh = timeout;
}

void ManageCenter::setMaxWrTimeout(
    GenData* data, unsigned timeout) {
    data->m_timer_ele[ENUM_DIR_SENDER].m_timeout_thresh = timeout;
}

void ManageCenter::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    m_rd_timeout = rd_timeout;
    m_wr_timeout = wr_timeout;
}

void ManageCenter::setAddr(GenData* data, 
    const char szIP[], int port) {
    data->m_port = port;
    MiscTool::strCpy(data->m_szIP, szIP, sizeof(data->m_szIP));
}

int ManageCenter::getAddr(int fd, int* pPort,
    char ip[], int max) {
    GenData* data = NULL;

    if (exists(fd)) {
        data = find(fd);

        if (NULL != pPort) {
            *pPort = data->m_port;
        }

        if (NULL != ip && 0 < max) {
            MiscTool::strCpy(ip, data->m_szIP, max);
        }
        
        return 0; 
    } else {
        if (NULL != pPort) {
            *pPort = 0;
        }

        if (NULL != ip && 0 < max) {
            ip[0] = '\0';
        }
        
        return -1;
    }
}

void ManageCenter::setData(GenData* data,
    ISockBase* sock, long extra) {
    data->m_sock = sock;
    data->m_extra = extra; 
}

GenData* ManageCenter::reg(int fd) {
    void* ele = NULL;
    GenData* data = NULL; 
    bool bOk = false;

    if (0 < fd && fd < m_cap) {
        lock();
        if (NULL == m_datas[fd]) {
            bOk = pop(&m_pool, &ele); 
            if (bOk) { 
                data = reinterpret_cast<GenData*>(ele); 
                m_datas[fd] = data; 
            } 
        } 
        unlock();

        if (bOk) {
            _allocData(data);
            data->m_fd = fd; 
            
            return data;
        } else {
            LOG_WARN("reg_fd| fd=%d| capacity=%d|"
                " msg=busy fd|",
                fd, m_cap);
            return NULL;
        } 
    } else {
        LOG_WARN("reg_fd| fd=%d| capacity=%d|"
            " msg=invalid fd|",
            fd, m_cap);
        return NULL;
    } 
} 

bool ManageCenter::unreg(GenData* data) {
    int fd = data->m_fd;
    
    if (0 < fd && fd < m_cap && data == m_datas[fd]) {
        LOG_INFO("fd=%d| addr=%s:%d| cnt=%u| msg=closed|", 
            fd, data->m_szIP, data->m_port, 
            data->m_closing);
        
        _releaseData(data);
        
        lock();
        m_datas[fd] = NULL;
        data->m_fd = -100000;

        push(&m_pool, data);
        unlock(); 

        return true;
    } else {
        LOG_ERROR("data=%p| fd=%d| msg=invalid data|",
            data, fd);

        return false;
    }
}

bool ManageCenter::updateExpire(EnumDir enDir, 
    GenData* data, unsigned now, bool force) {
    TimerEle* ele = &data->m_timer_ele[enDir];
    bool action = false;

    if (0 < ele->m_timeout_thresh) {
        if (now != ele->m_updated_time || force) {
            ele->m_updated_time = now;
            action = true;
        }
    }

    return action;
}

void ManageCenter::releaseMsgQue(LList* list) {
    NodeMsg* base = NULL;
    LList* node = NULL;
    LList* n = NULL; 
    
    if (!isEmpty(list)) {
        for_each_list_safe(node, n, list) {
            base = NodeUtil::toNode(node);
            del(node);
            NodeUtil::freeNode(base);
        }
    }
}

int ManageCenter::addMsg(EnumDir enDir,
    GenData* data, NodeMsg* pMsg) {
    int ret = 0;
    
    switch (enDir) { 
    case ENUM_DIR_SENDER:
        NodeUtil::queue(&data->m_wr_msg_que, pMsg);
        break;

    case ENUM_DIR_DEALER:
        NodeUtil::queue(&data->m_deal_msg_que, pMsg);
        break;
        
    default:
        ret = -1;
        break;
    }

    return ret;
}

LList* ManageCenter::getWrQue(GenData* data) {
    if (!isEmpty(&data->m_wr_msg_que)) {
        append(&data->m_wr_msg_que_priv, &data->m_wr_msg_que);
    }
    
    return &data->m_wr_msg_que_priv;
}

LList* ManageCenter::getDealQue(GenData* data) {
    if (!isEmpty(&data->m_deal_msg_que)) {
        append(&data->m_deal_msg_que_priv, &data->m_deal_msg_que);
    }
    
    return &data->m_deal_msg_que_priv;
}

int ManageCenter::onRecv(GenData* data, 
    const char* buf, int size, const SockAddr* addr) {
    ISockBase* comm = NULL;
    int fd = data->m_fd;
    int ret = 0; 

    comm = data->m_sock;
    if (NULL != comm) {
        ret = comm->parseData(fd, buf, size, addr); 
    }

    return ret;
}

int ManageCenter::onNewSock(GenData* newData,
    GenData* parent, AccptOption& opt) {
    ISockSvr* svr = NULL;
    int ret = 0; 
    int listenFd = getFd(parent);
    int newFd = getFd(newData);

    svr = dynamic_cast<ISockSvr*>(parent->m_sock);
    if (NULL != svr) { 
        ret = svr->onNewSock(listenFd, newFd, opt);
        if (0 == ret) { 
            setData(newData, svr, opt.m_extra);
        }
    } else {
        ret = -1;
    }

    return ret;
}

int ManageCenter::onConnect(GenData* data, 
    ConnOption& opt) {
    ISockCli* cli = NULL; 
    int fd = data->m_fd;
    int ret = 0;

    cli = dynamic_cast<ISockCli*>(data->m_sock);
    if (NULL != cli) {
        if (0 == data->m_wr_conn) { 
            LOG_INFO("connect_ok| newfd=%d| peer=%s:%d|", 
                fd, data->m_szIP, data->m_port);
            
            ret = cli->onConnOK(fd, opt);
        } else {
            LOG_INFO("connect_err| newfd=%d| peer=%s:%d|", 
                    fd, data->m_szIP, data->m_port);
        
            ret = data->m_wr_conn;
            cli->onConnFail(data->m_extra, ret);
        }
    } else {
        ret = -1;
    }

    return ret;
}

int ManageCenter::onProcess(GenData* data, NodeMsg* msg) {
    ISockBase* comm = NULL;
    int fd = data->m_fd;
    int ret = 0;

    comm = data->m_sock;
    if (NULL != comm) {
        ret = comm->process(fd, msg);
        if (0 != ret) {
            LOG_INFO("on_process| fd=%d| ret=%d|"
                " msg=proc msg error|", 
                fd, ret);
        }
    }
    
    return ret;
}

void ManageCenter::onClose(GenData* data) {
    ISockBase* comm = NULL;
    ISockSvr* svr = NULL;
    int fd = data->m_fd;

    if (ENUM_DEAL_SOCK == data->m_deal_cb) {
        comm = data->m_sock;
        if (NULL != comm) {
            comm->onClose(fd);
        }
    } else if (ENUM_DEAL_LISTENER == data->m_deal_cb) {
        svr = dynamic_cast<ISockSvr*>(data->m_sock);

        if (NULL != svr) {
            svr->onListenerClose(fd);
        }
    } else {
        /* do nothing */
    }
}

