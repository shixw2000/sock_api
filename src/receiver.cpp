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
    m_ev_fd[0] = m_ev_fd[1] = 0;
    m_timer_fd = 0;
    m_busy = false;
    m_tick = 0;
}

Receiver::~Receiver() {
}

int Receiver::init() {
    int ret = 0;
    int cap = m_center->capacity();

    ret = MiscTool::creatPipes(&m_ev_fd);
    if (0 != ret) {
        return -1;
    } 

    ret = MiscTool::creatTimer(DEF_TICK_MSEC);
    if (0 < ret) {
        m_timer_fd = ret;
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

    m_pfds[0].fd = m_ev_fd[0];
    m_pfds[1].fd = m_timer_fd;
    m_size = 2;

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

    if (0 < m_ev_fd[1]) {
        SockTool::closeSock(m_ev_fd[1]);
        m_ev_fd[1] = 0;
    }

    if (0 < m_ev_fd[0]) {
        SockTool::closeSock(m_ev_fd[0]);
        m_ev_fd[0] = 0;
    } 

    if (0 < m_timer_fd) {
        SockTool::closeSock(m_timer_fd);
        m_timer_fd = 0;
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
    int timeout = DEF_POLL_TIME_MSEC;
    int cnt = 0;
    unsigned tick = 0;
    GenData* data = NULL;
    
    cnt = poll(m_pfds, m_size, timeout);
    if (0 == cnt) {
        /* no event */
        return false;
    } else if (0 < cnt) { 
        if (0 != m_pfds[0].revents) {
            MiscTool::readPipeEvent(m_ev_fd[0]);
            
            /* reset the flag */
            m_pfds[0].revents = 0; 
            --cnt;
        }

        if (0 != m_pfds[1].revents) {
            tick = (unsigned)MiscTool::read8Bytes(m_timer_fd);
            if (0 < tick) { 
                m_tick = tick;
            }

            /* reset the flag */
            m_pfds[1].revents = 0; 
            --cnt;
        }
        
        for (int i=2; i<m_size && 0 < cnt; ++i) {
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

void Receiver::readListener(GenData* data) {
    int index = m_center->getRdIndex(data);
    
    if (0 < m_pfds[index].fd) {
        setStat(data, ENUM_STAT_DISABLE);
        _disableFd(data);

        /* send a accept cmd to deal thread */
        m_director->activate(ENUM_DIR_DEALER, data);
    } else {
        _enableFd(data);
        setStat(data, ENUM_STAT_BLOCKING);
    }
} 

void Receiver::flowCtlCallback(GenData* data) { 
    LOG_DEBUG("<== end rd flowctl| fd=%d| now=%u|",
        m_center->getFd(data), m_timer->now()); 

    _enableFd(data);
    _activate(data, ENUM_STAT_FLOWCTL);
}

void Receiver::flowCtl(GenData* data, unsigned total) { 
    (void)total;
    LOG_DEBUG("==> begin rd flowctl| fd=%d| now=%u| total=%u|",
        m_center->getFd(data), m_timer->now(), total);
    
    /* disable read */
    _disableFd(data);
    setStat(data, ENUM_STAT_FLOWCTL);

    m_center->flowctl(ENUM_DIR_RECVER, m_timer, data);
}

void Receiver::readDefault(GenData* data) {
    LOG_INFO("fd=%d| stat=%d| cb=%d| msg=invalid read cb|", 
        m_center->getFd(data), 
        m_center->getStat(ENUM_DIR_RECVER, data), 
        m_center->getCb(ENUM_DIR_RECVER, data)); 

    detach(data, ENUM_STAT_ERROR);
    
    _disableFd(data); 
    
    m_director->notifyClose(data);
}

void Receiver::readSock(GenData* data) {
    unsigned long long now = m_timer->now();
    int fd = m_center->getFd(data);
    int ret = 0;
    unsigned max = 0;
    unsigned threshold = 0;
    unsigned total = 0;

    m_center->clearBytes(ENUM_DIR_RECVER, data, now);
    threshold = m_center->getRdThresh(data); 
    
    if (0 < threshold) { 
        max = m_center->calcThresh(ENUM_DIR_RECVER, data, now);
        if (0 == max) {
            flowCtl(data, 0);
            return;
        }
    } else {
        max = 0;
    }
    
    do {
        ret = m_center->recvMsg(data, max);
        if (0 < ret) { 
            total += ret;
            
            if (0 < max && total >= max) {
                break; 
            } 
        }
    } while (0 < ret);

    if (0 <= ret) {
        if (0 < total) {
            LOG_DEBUG("read_sock| fd=%d| ret=%d| max=%u| total=%u|",
                fd, ret, max, total); 
        
            m_center->recordBytes(ENUM_DIR_RECVER, data, now, total);
            addFlashTimeout(now, &m_time_flash_queue, data);
        }
        
        if (0 < ret) {
            flowCtl(data, total);
        } else if (0 == ret) {
            setStat(data, ENUM_STAT_BLOCKING);
        }
    } else {
        LOG_INFO("read_sock| fd=%d| ret=%d| max=%u| total=%u| msg=close|",
            fd, ret, max, total);
            
        if (-2 == ret) {
            detach(data, ENUM_STAT_CLOSING);
        } else {
            detach(data, ENUM_STAT_ERROR);
        }
        
        _disableFd(data); 
        
        m_director->notifyClose(data);
    } 
}

void Receiver::dealRunQue(LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        data = m_center->fromNode(ENUM_DIR_RECVER, node);

        m_center->delNode(ENUM_DIR_RECVER, data);
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
        data = m_center->fromTimeout(ENUM_DIR_RECVER, node);

        expired = m_center->chkExpire(ENUM_DIR_RECVER, data, now);
        if (!expired) {
            break;
        } else {
            LOG_INFO("flash_timeout| fd=%d| msg=read close|",
                m_center->getFd(data));
            
            detach(data, ENUM_STAT_TIMEOUT); 

            _disableFd(data);
            
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
        m_center->delTimeout(ENUM_DIR_RECVER, data);
        m_center->addTimeout(ENUM_DIR_RECVER, list, data);
    }
}

void Receiver::callback(GenData* data) {
    int cb = m_center->getCb(ENUM_DIR_RECVER, data);
    
    if (0 <= cb && ENUM_RD_END > cb) {
        (this->*(m_func[cb]))(data); 
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
    unsigned tick = 0;
    LList runlist = INIT_LIST(runlist);
    LList cmdlist = INIT_LIST(cmdlist);
    
    lock();
    if (!isEmpty(&m_run_queue)) {
        append(&runlist, &m_run_queue);
    }

    if (!isEmpty(&m_cmd_queue)) {
        append(&cmdlist, &m_cmd_queue);
    }

    if (0 < m_tick) {
        tick = m_tick;
        m_tick = 0;
    }

    if (m_busy) {
        m_busy = false;
    }
    unlock();

    if (!isEmpty(&runlist)) {
        dealRunQue(&runlist);
    } 

    if (0 < tick) {
        m_director->notifyTimer(ENUM_DIR_SENDER, tick); 
        m_director->notifyTimer(ENUM_DIR_DEALER, tick); 
        m_timer->run(tick);
    }

    if (!isEmpty(&cmdlist)) {
        dealCmds(&cmdlist);
    }
}

bool Receiver::_queue(GenData* data, int expectStat) {
    bool action = false;
    int stat = m_center->getStat(ENUM_DIR_RECVER, data);

    if (expectStat == stat) {
        setStat(data, ENUM_STAT_READY);
        m_center->addNode(ENUM_DIR_RECVER, &m_run_queue, data);
        
        if (!m_busy) {
            m_busy = true;
            action = true;
        } 
    }

    return action;
}

bool Receiver::queue(GenData* data) {
    bool action = false;

    LOG_DEBUG("fd=%d| msg=read event|", m_center->getFd(data));
    
    lock();
    action = _queue(data, ENUM_STAT_BLOCKING);
    unlock();

    return action;
}

void Receiver::detach(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    m_center->delNode(ENUM_DIR_RECVER, data); 
    unlock();

    m_center->delTimeout(ENUM_DIR_RECVER, data);
    m_center->unflowctl(ENUM_DIR_RECVER, data);
}

void Receiver::_disableFd(GenData* data) {
    int fd = m_center->getFd(data);
    int index = m_center->getRdIndex(data);

    if (0 < fd) {
        m_pfds[index].fd = -fd;
    }
}

void Receiver::_enableFd(GenData* data) {
    int fd = m_center->getFd(data);
    int index = m_center->getRdIndex(data);

    if (0 < fd) {
        m_pfds[index].fd = fd;
    }
}

void Receiver::signal() {
    MiscTool::writePipeEvent(m_ev_fd[1]);
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

int Receiver::_activate(GenData* data, int stat) {
    bool action = false;

    lock();
    action = _queue(data, stat); 
    unlock();
    
    if (action) {
        signal();
    }

    return 0;
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
    int cb = 0;
    int stat = 0;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base); 
    fd = pCmd->m_fd; 
    
    if (exists(fd)) {
        data = find(fd);
            
        cb = m_center->getCb(ENUM_DIR_RECVER, data);
        stat = m_center->getStat(ENUM_DIR_RECVER, data);
        
        if(ENUM_STAT_INIT == stat) {
            m_pfds[m_size].fd = fd;
            m_pfds[m_size].events = POLLIN;
            m_pfds[m_size].revents = 0;

            m_center->setRdIndex(data, m_size);
            ++m_size;
            
            setStat(data, ENUM_STAT_BLOCKING);

            if (ENUM_RD_SOCK == cb) {
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

        detach(data, ENUM_STAT_INVALID);
    
        index = m_center->getRdIndex(data);
        m_center->setRdIndex(data, 0);

        --m_size;
        if (index < m_size) {
            /* fill the last into this hole */
            lastFd = m_pfds[m_size].fd;

            if (0 < lastFd) {
                dataLast = find(lastFd);
            } else {
                dataLast = find(-lastFd);
            }
            
            m_center->setRdIndex(dataLast, index);
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

