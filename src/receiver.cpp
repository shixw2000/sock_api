#include<cstdlib>
#include<cstring>
#include"shareheader.h"
#include"msgutil.h"
#include"director.h"
#include"receiver.h" 
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"
#include"misc.h"


void Receiver::Recv1SecCb(long data1, long) {
    Receiver* receiver = (Receiver*)data1;

    receiver->cbTimer1Sec();
}

Receiver::PRdFunc Receiver::m_func[ENUM_RD_END] = {
    &Receiver::readDefault,
    &Receiver::readSock,
    &Receiver::readListener,
    &Receiver::readTimer,
};

Receiver::Receiver(ManageCenter* center, Director* director) {
    m_center = center;
    m_director = director;
    m_timer = NULL;
    m_lock = NULL;
    m_pfds = NULL;
    m_1sec_obj = NULL;

    initList(&m_run_queue);
    initList(&m_cmd_queue);
    initList(&m_time_flash_queue);
    
    m_size = 0;
    m_ev_fd = 0;
    m_busy = false;
}

Receiver::~Receiver() {
}

int Receiver::init() {
    int ret = 0;
    int cap = m_center->capacity();

    ret = MiscTool::creatEvent();
    if (0 < ret) {
        m_ev_fd = ret;
    } else {
        return -1;
    }

    m_lock = new SpinLock; 
    
    m_pfds = (struct pollfd*)calloc(cap, sizeof(struct pollfd));
    for (int i=0; i<cap; ++i) {
        m_pfds[i].events = POLLIN;
    } 

    m_timer = new TickTimer; 
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, &Receiver::Recv1SecCb, (long)this);
    startTimer1Sec();

    m_size = 0;
    m_pfds[m_size++].fd = m_ev_fd;

    return ret;
}

void Receiver::finish() { 
    if (NULL != m_1sec_obj) {
        TickTimer::freeObj(m_1sec_obj);
        m_1sec_obj = NULL;
    }
    
    if (NULL != m_timer) {
        delete m_timer;
        m_timer = NULL;
    }
    
    if (NULL != m_pfds) {
        free(m_pfds);
        m_pfds = NULL;
    }
    
    if (NULL != m_lock) {
        delete m_lock;
        m_lock = NULL;
    }

    if (0 < m_ev_fd) {
        SockTool::closeSock(m_ev_fd);
        m_ev_fd = 0;
    }
}

GenData* Receiver::find(int fd) const {
    return m_center->find(fd);
}

void Receiver::run() {
    while (isRun()) { 
        doTasks();
    }
}

void Receiver::doTasks() { 
    wait(); 
    consume();
}

bool Receiver::wait() {
    int timeout = 1000;
    int cnt = 0;
    GenData* data = NULL;
    
    cnt = poll(m_pfds, m_size, timeout);
    if (0 == cnt) {
        /* no event */
        return false;
    } else if (0 < cnt) { 
        if (0 != m_pfds[0].revents) {
            MiscTool::read8Bytes(m_ev_fd);
            
            /* reset the flag */
            m_pfds[0].revents = 0; 
            --cnt;
        }
        
        for (int i=1; i<m_size && 0 < cnt; ++i) {
            if (0 != m_pfds[i].revents) { 
                data = find(m_pfds[i].fd); 
                    
                queue(data);
                
                /* reset the flag */
                m_pfds[i].revents = 0; 
                --cnt;
            }
        } 

        return true;
    } else {
        /* error */
        return false;
    }
}

void Receiver::readTimer(GenData* data) {
    unsigned long long ullTick = 0;
    unsigned tick = 0;
    int fd = m_center->getFd(data); 
    
    lockSet(data, ENUM_STAT_BLOCKING);
    ullTick = MiscTool::read8Bytes(fd);
    if (0 < ullTick) {
        tick = (unsigned)ullTick;
        
        m_director->notifyTimer(ENUM_DIR_SENDER, data, tick); 
        m_director->notifyTimer(ENUM_DIR_DEALER, data, tick); 
        m_timer->run(tick);
    } 
}

void Receiver::readListener(GenData* data) {
    int index = data->m_rd_index;
    
    if (0 < m_pfds[index].fd) {
        lockSet(data, ENUM_STAT_DISABLE);
        _disableFd(data);

        /* send a accept cmd to deal thread */
        m_director->activate(ENUM_DIR_DEALER, data);
    } else {
        _enableFd(data);
        lockSet(data, ENUM_STAT_BLOCKING);
    }
} 

void Receiver::flowCtlCallback(GenData* data) { 
    LOG_DEBUG("<== end rd flowctl| fd=%d| now=%u|",
        data->m_fd, m_timer->now());
    
    /* enable read */
    _enableFd(data);
    
    _activate(data, ENUM_STAT_FLOWCTL);
}

void Receiver::flowCtl(GenData* data, unsigned total) { 
    (void)total;
    LOG_DEBUG("==> begin rd flowctl| fd=%d| now=%u| total=%u|",
        data->m_fd, m_timer->now(), total);
    
    /* disable read */
    _disableFd(data);
    lockSet(data, ENUM_STAT_FLOWCTL);
    
    m_timer->schedule(&data->m_rd_flowctl, DEF_FLOWCTL_TICK_NUM);
}

void Receiver::readDefault(GenData* data) {
    int fd = m_center->getFd(data);
    
    LOG_INFO("fd=%d| stat=%d| cb=%d| read default", 
        fd, data->m_rd_stat, data->m_rd_cb); 

    _disableFd(data);
    lockSet(data, ENUM_STAT_ERROR);
    
    m_director->notifyClose(data);
}

void Receiver::readSock(GenData* data) {
    int fd = data->m_fd;
    unsigned long long now = m_timer->now();
    SockBuffer* buffer = &data->m_rd_buf;
    int ret = 0;
    unsigned max = 0;
    unsigned threshold = 0;
    unsigned total = 0;
    EnumSockStat stat = ENUM_STAT_READY; 

    m_center->clearBytes(ENUM_DIR_RECVER, data, now);
    threshold = m_center->getRdThresh(data); 
    
    if (0 < threshold) { 
        max = m_center->calcThresh(ENUM_DIR_RECVER, data, now);
        if (0 == max) {
            stat = ENUM_STAT_FLOWCTL;
            flowCtl(data, 0);
        }
    } else {
        max = 0;
    }
    
    while (ENUM_STAT_READY == stat) {
        ret = m_center->recvMsg(fd, buffer, max);
        if (0 < ret) { 
            total += ret;
            
            if (0 < max && total >= max) {
                stat = ENUM_STAT_FLOWCTL;
                flowCtl(data, total);
            } 
        } else if (0 == ret) {
            stat = ENUM_STAT_BLOCKING;
            lockSet(data, ENUM_STAT_BLOCKING);
        } else if (-2 == ret) {
            stat = ENUM_STAT_CLOSING;
            readDefault(data);
        } else {
            stat = ENUM_STAT_ERROR;
            readDefault(data);
        }
    }

    postSendData(now, data, stat, max, total);
}

void Receiver::postSendData(unsigned now, GenData* data,
    int stat, unsigned max, unsigned total) {
    
    if (0 < total) {
        m_center->recordBytes(ENUM_DIR_RECVER, data, now, total);

        addFlashTimeout(now, &m_time_flash_queue, data);

        (void)stat;
        (void)max;
        LOG_DEBUG("read_sock| fd=%d| stat=%d| max=%u| total=%u|",
            m_center->getFd(data), stat, max, total);  
    }
}

void Receiver::dealRunQue(LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        data = CONTAINER(node, GenData, m_rd_node);

        del(&data->m_rd_node); 
        callback(data);
    } 
}

void Receiver::cbTimer1Sec() {
    unsigned now = m_timer->now();
    
    startTimer1Sec();

    LOG_DEBUG("=======recver_now=%u|", now);
    
    if (!isEmpty(&m_time_flash_queue)) {
        dealFlashTimeout(now, &m_time_flash_queue);
    }
}

void Receiver::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, DEF_NUM_PER_SEC);
}

void Receiver::dealFlashTimeout(unsigned now, LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;
    bool expired = false;

    for_each_list_safe(node, n, list) {
        data = CONTAINER(node, GenData, m_rd_timeout_node);

        expired = m_center->chkExpire(ENUM_DIR_RECVER, data, now);
        if (!expired) {
            break;
        } else {
            del(&data->m_rd_timeout_node); 
            m_director->notifyClose(data);
        }
    } 
}

void Receiver::addFlashTimeout(unsigned now, 
    LList* list, GenData* data) {
    bool action = false;
    
    /* timeout check */
    action = m_center->updateExpire(ENUM_DIR_RECVER, data, now);
    if (action) {
        del(&data->m_rd_timeout_node);
        add(list, &data->m_rd_timeout_node);
    }
}

void Receiver::callback(GenData* data) {
    if (0 <= data->m_rd_cb && ENUM_RD_END > data->m_rd_cb) {
        (this->*(m_func[data->m_rd_cb]))(data); 
    } else {
        (this->*(m_func[ENUM_RD_DEFAULT]))(data); 
    }
}

void Receiver::dealCmds(LList* list) {
    NodeCmd* base = NULL;
    LList* n = NULL;
    LList* node = NULL;

    for_each_list_safe(node, n, list) {
        base = MsgUtil::toNodeCmd(node);
        del(node);

        procCmd(base);
        MsgUtil::freeNodeCmd(base);
    }
}

void Receiver::setStat(GenData* data, int stat) {
    m_center->setStat(ENUM_DIR_RECVER, data, stat); 
}

void Receiver::consume() {
    LList runlist = INIT_LIST(runlist);
    LList cmdlist = INIT_LIST(cmdlist);
    
    lock();
    if (!isEmpty(&m_run_queue)) {
        append(&runlist, &m_run_queue);
    }

    if (!isEmpty(&m_cmd_queue)) {
        append(&cmdlist, &m_cmd_queue);
    }

    if (m_busy) {
        m_busy = false;
    }
    unlock();

    if (!isEmpty(&runlist)) {
        dealRunQue(&runlist);
    } 

    if (!isEmpty(&cmdlist)) {
        dealCmds(&cmdlist);
    }
}

bool Receiver::_queue(GenData* data, int expectStat) {
    bool action = false;

    if (expectStat == data->m_rd_stat) {
        setStat(data, ENUM_STAT_READY);
        
        if (!m_busy) {
            m_busy = true;
            action = true;
        }
        
        add(&m_run_queue, &data->m_rd_node);
    }

    return action;
}

bool Receiver::queue(GenData* data) {
    bool action = false;
    
    lock();
    action = _queue(data, ENUM_STAT_BLOCKING);
    unlock();

    return action;
}

void Receiver::lockSet(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    unlock();
}

void Receiver::_disableFd(GenData* data) {
    int fd = m_center->getFd(data);
    int index = data->m_rd_index;

    if (0 < fd) {
        m_pfds[index].fd = -fd;
    }
}

void Receiver::_enableFd(GenData* data) {
    int fd = m_center->getFd(data);
    int index = data->m_rd_index;

    if (0 < fd) {
        m_pfds[index].fd = fd;
    }
}

void Receiver::signal() {
    MiscTool::writeEvent(m_ev_fd);
}

int Receiver::_activate(GenData* data, int expectStat) {
    bool action = false;

    lock();
    action = _queue(data, expectStat); 
    unlock();
    
    if (action) {
        signal();
    }

    return 0;
}

int Receiver::addCmd(NodeCmd* pCmd) {
    bool action = false;

    lock(); 
    MsgUtil::addNodeCmd(&m_cmd_queue, pCmd);
    if (!m_busy) {
        m_busy = true;
        action = true;
    }
    unlock(); 

    if (action) {
        signal();
    }

    return 0;
}

int Receiver::activate(GenData* data) {
    int ret = 0;

    ret = _activate(data, ENUM_STAT_DISABLE);
    return ret;
}

void Receiver::procCmd(NodeCmd* base) { 
    CmdHead_t* pHead = NULL;

    pHead = MsgUtil::getCmd(base);
    switch (pHead->m_cmd) { 
    case ENUM_CMD_ADD_FD:
        cmdAddFd(base);
        break;

    case ENUM_CMD_REMOVE_FD:
        cmdRemoveFd(base);
        break;

    default:
        break;
    }
}

void Receiver::lock() {
    m_lock->lock();
}

void Receiver::unlock() {
    m_lock->unlock();
}

void Receiver::cmdAddFd(NodeCmd* base) {
    int fd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base); 
    fd = pCmd->m_fd; 
    
    if (exists(fd)) {
        data = find(fd);
            
        if(ENUM_STAT_INVALID == data->m_rd_stat) {
            lock();
            m_pfds[m_size].fd = fd;
            m_pfds[m_size].events = POLLIN;
            m_pfds[m_size].revents = 0;

            data->m_rd_index = m_size;
            ++m_size;
            
            setStat(data, ENUM_STAT_BLOCKING);
            unlock(); 

            if (ENUM_RD_SOCK == data->m_rd_cb) {
                addFlashTimeout(m_timer->now(), 
                    &m_time_flash_queue, data);
            }
        } 
    }
}

/* the first step of close a fd */
void Receiver::cmdRemoveFd(NodeCmd* base) { 
    int fd = -1;
    int index = -1;
    int lastFd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;
    GenData* dataLast = NULL;
    NodeCmd* refCmd = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base); 
    fd = pCmd->m_fd; 
    
    LOG_INFO("receiver_remove_fd| fd=%d|", fd);
    
    if (exists(fd)) { 
        data = find(fd); 

        lock(); 
        setStat(data, ENUM_STAT_INVALID); 
        del(&data->m_rd_node); 
        unlock();
        
        del(&data->m_rd_timeout_node);
        TickTimer::delTimer(&data->m_rd_flowctl);
    
        index = data->m_rd_index;
        data->m_rd_index = 0; 

        --m_size;
        if (index < m_size) {
            /* fill the last into this hole */
            lastFd = m_pfds[m_size].fd;

            if (0 < lastFd) {
                dataLast = find(lastFd);
            } else {
                dataLast = find(-lastFd);
            }
            
            dataLast->m_rd_index = index;
            m_pfds[index].fd = lastFd;
        } 

        m_pfds[m_size].fd = 0;
        
        /* go to the second step of closing */
        refCmd = MsgUtil::refNodeCmd(base);
        m_director->sendCmd(ENUM_DIR_SENDER, refCmd);
    }
}

bool Receiver::exists(int fd) const {
    return m_center->exists(fd);
}

