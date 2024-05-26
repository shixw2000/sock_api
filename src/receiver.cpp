#include<cstdlib>
#include<cstring>
#include"shareheader.h"
#include"msgtool.h"
#include"director.h"
#include"receiver.h" 
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"
#include"misc.h"
#include"socktool.h"
#include"cmdcache.h"
#include"nodebase.h"
#include"cache.h"


bool Receiver::recvSecCb(long data1, long, TimerObj*) {
    Receiver* receiver = (Receiver*)data1;
    
    receiver->cbTimer1Sec();
    return true;
}

bool Receiver::recvTimeoutCb(long p1, 
    long p2, TimerObj*) {
    Receiver* receiver = (Receiver*)p1;
    GenData* data = (GenData*)p2;
    
    receiver->dealTimeoutCb(data); 
    return false;
}


Receiver::PRdFunc Receiver::m_func[ENUM_RD_END] = {
    &Receiver::readDefault,
    &Receiver::readSock,
    &Receiver::readListener,
    &Receiver::readUdp,
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
    
    m_size = 0;
    m_ev_fd[0] = m_ev_fd[1] = 0;
    m_timer_fd = 0;
    m_busy = false;
    m_tick = 0;
    m_now_sec = 0;
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
    
    m_pfds = (struct pollfd*)CacheUtil::callocAlign(
        cap, sizeof(struct pollfd));
    for (int i=0; i<cap; ++i) {
        m_pfds[i].events = POLLIN;
    } 

    m_timer = new TickTimer; 
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, 
        &Receiver::recvSecCb, (long)this);
    
    startTimer1Sec();

    m_pfds[0].fd = m_ev_fd[0];
    m_pfds[1].fd = m_timer_fd;
    m_size = MIN_RCV_PFD;

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
        CacheUtil::freeAlign(m_pfds);
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
    LList runlist = INIT_LIST(runlist);
    
    wait(&runlist); 
    consume(&runlist);
}

bool Receiver::wait(LList* runlist) {
    int timeout = DEF_POLL_TIME_MSEC;
    int cnt = 0;
    unsigned tick = 0;
    GenData* data = NULL;

    if (m_busy) {
        timeout = 0;
    }
    
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
        
        for (int i=MIN_RCV_PFD; i<m_size && 0 < cnt; ++i) {
            if (0 != m_pfds[i].revents) { 
                data = find(m_pfds[i].fd); 
                    
                toRead(runlist, data);
                
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

void Receiver::flowCtlCb(GenData* data) { 
    LOG_DEBUG("<== end rd flowctl| fd=%d| now=%u|",
        m_center->getFd(data), m_timer->now()); 

    _enableFd(data);
    _activate(data, ENUM_STAT_FLOWCTL);

    /* switch timeout */
    addFlashTimeout(data, true);
}

void Receiver::flowCtl(GenData* data, unsigned total) { 
    (void)total;
    LOG_DEBUG("==> begin rd flowctl| fd=%d| now=%u| total=%u|",
        m_center->getFd(data), m_timer->now(), total);
    
    /* disable read */
    _disableFd(data);

    m_center->cancelTimer(ENUM_DIR_RECVER, data);
    m_center->enableTimer(ENUM_DIR_RECVER, m_timer, data,
        ENUM_TIMER_EVENT_FLOWCTL,
        DEF_FLOWCTL_TICK_NUM);
        
    setStat(data, ENUM_STAT_FLOWCTL);
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
    unsigned now = m_timer->now();
    int fd = m_center->getFd(data);
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    unsigned max = 0;
    unsigned threshold = 0;
    int total = 0;

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
 
    ret = recvTcp(data, max, &total);
    if (0 < total) {
        LOG_DEBUG("read_sock| fd=%d| ret=%d| max=%u| total=%u|",
            fd, ret, max, total); 
    
        m_center->recordBytes(ENUM_DIR_RECVER, data, now, total); 
        addFlashTimeout(data);
    }
    
    if (ENUM_SOCK_RET_OK == ret) {
        /* for next run loop */
        if (!m_busy) {
            m_busy = true;
        }
    } else if (ENUM_SOCK_RET_BLOCK == ret) {
        setStat(data, ENUM_STAT_BLOCKING); 
    } else { 
        detach(data, ENUM_STAT_ERROR);
        _disableFd(data); 
        
        m_director->notifyClose(data);
    }
}

void Receiver::readUdp(GenData* data) {
    unsigned now = m_timer->now();
    int fd = m_center->getFd(data);
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int total = 0;
    
    ret = recvUdp(data, 0, &total);
    if (0 < total) {
        LOG_DEBUG("read_upd| fd=%d| ret=%d| total=%d|",
            fd, ret, total);
        
        m_center->recordBytes(ENUM_DIR_RECVER, data, now, total);
    }
    
    if (ENUM_SOCK_RET_OK == ret) {
        /* for next run loop */
        if (!m_busy) {
            m_busy = true;
        }
    } else if (ENUM_SOCK_RET_BLOCK == ret) { 
        setStat(data, ENUM_STAT_BLOCKING);
    } else { 
        detach(data, ENUM_STAT_CLOSING);
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
    ++m_now_sec;
    LOG_DEBUG("=======recver_now=%u|", m_now_sec);
}

void Receiver::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, 0, DEF_NUM_PER_SEC);
}

void Receiver::dealTimeoutCb(GenData* data) { 
    int event = 0;

    event = ManageCenter::getEvent(ENUM_DIR_RECVER, data); 
    switch (event) { 
    case ENUM_TIMER_EVENT_TIMEOUT:
        LOG_INFO("flash_timeout| fd=%d| msg=read close|",
            m_center->getFd(data));
        
        detach(data, ENUM_STAT_TIMEOUT); 
        _disableFd(data); 
        m_director->notifyClose(data); 
        break;

    case ENUM_TIMER_EVENT_FLOWCTL:
        flowCtlCb(data);
        break;

    default:
        break;
    }
    
    
}

void Receiver::addFlashTimeout(
    GenData* data, bool force) {
    bool action = false;
    
    /* timeout check */
    action = m_center->updateExpire(ENUM_DIR_RECVER, 
        data, m_now_sec, force);
    if (action) {
        m_center->cancelTimer(ENUM_DIR_RECVER, data);
        m_center->enableTimer(ENUM_DIR_RECVER, m_timer, 
            data, ENUM_TIMER_EVENT_TIMEOUT);
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
    NodeMsg* base = NULL;
    LList* n = NULL;
    LList* node = NULL;

    for_each_list_safe(node, n, list) {
        base = NodeUtil::toNode(node);
        del(node);

        procCmd(base);
        NodeUtil::freeNode(base);
    }
}

void Receiver::setStat(GenData* data, int stat) {
    m_center->setStat(ENUM_DIR_RECVER, data, stat); 
}

void Receiver::consume(LList* runlist) {
    unsigned tick = 0;
    LList cmdlist = INIT_LIST(cmdlist);
    
    lock();
    if (!isEmpty(&m_run_queue)) {
        append(runlist, &m_run_queue);
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

    if (!isEmpty(runlist)) {
        dealRunQue(runlist);
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

void Receiver::toRead(LList* runlist, GenData* data) { 
    /* delete from wait queue, no need lock */
    setStat(data, ENUM_STAT_READY);
    m_center->delNode(ENUM_DIR_RECVER, data);

    /* add to run queue */
    m_center->addNode(ENUM_DIR_RECVER, runlist, data); 
    if (!m_busy) {
        m_busy = true;
    }
}

void Receiver::detach(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    m_center->delNode(ENUM_DIR_RECVER, data); 
    unlock();

    m_center->cancelTimer(ENUM_DIR_RECVER, data);
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

int Receiver::addCmd(NodeMsg* pCmd) {
    bool action = false;

    lock(); 
    NodeUtil::queue(&m_cmd_queue, pCmd);
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

void Receiver::procCmd(NodeMsg* base) { 
    int cmd = 0;

    cmd = CmdUtil::getCmd(base);
    switch (cmd) { 
    case ENUM_CMD_ADD_FD:
        cmdAddFd(base);
        break;

    case ENUM_CMD_DELAY_FD:
        cmdDelayFd(base);
        break;

    case ENUM_CMD_UNDELAY_FD:
        cmdUndelayFd(base);
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

void Receiver::cmdAddFd(NodeMsg* base) {
    _AddFd(base, false);
}

void Receiver::cmdDelayFd(NodeMsg* base) {
    _AddFd(base, true);
}

void Receiver::cmdUndelayFd(NodeMsg* base) {
    int fd = -1;
    int stat = 0;
    int index = 0;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;

    assert(exists(fd));
    
    if (exists(fd)) { 
        LOG_DEBUG("undelay_fd| fd=%d|", fd);
        data = find(fd); 
        index = m_center->getRdIndex(data);

        lock();
        stat = m_center->getStat(ENUM_DIR_RECVER, data);
        if (ENUM_STAT_DELAY == stat) {
            setStat(data, ENUM_STAT_BLOCKING); 
            m_pfds[index].fd = fd;
        }
        unlock(); 
    }
}

void Receiver::_AddFd(NodeMsg* base, bool delay) {
    int fd = -1;
    int cb = 0;
    int stat = 0;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base); 
    fd = pCmd->m_fd; 
    
    if (exists(fd)) {
        data = find(fd);
            
        cb = m_center->getCb(ENUM_DIR_RECVER, data);
        stat = m_center->getStat(ENUM_DIR_RECVER, data);
        
        assert(ENUM_STAT_INVALID == stat);
        
        if(ENUM_STAT_INVALID == stat) {
            
            m_pfds[m_size].events = POLLIN;
            m_pfds[m_size].revents = 0; 

            if (!delay) {
                m_pfds[m_size].fd = fd;
                setStat(data, ENUM_STAT_BLOCKING); 
            } else {
                m_pfds[m_size].fd = -fd;
                setStat(data, ENUM_STAT_DELAY);
            }

            if (ENUM_RD_SOCK == cb) { 
                addFlashTimeout(data, true);
            }

            m_center->setRdIndex(data, m_size);
            ++m_size;
        } 
    }
}

/* the first step of close a fd */
void Receiver::cmdRemoveFd(NodeMsg* base) { 
    int fd = -1;
    int index = -1;
    int lastFd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;
    GenData* dataLast = NULL;
    NodeMsg* refCmd = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base); 
    fd = pCmd->m_fd; 
    
    LOG_DEBUG("receiver_remove_fd| fd=%d|", fd);
    
    assert(exists(fd) && MIN_RCV_PFD < m_size);
    
    if (exists(fd)) { 
        data = find(fd); 

        detach(data, ENUM_STAT_INVALID);
    
        index = m_center->getRdIndex(data);
        m_center->setRdIndex(data, -70000);

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
        refCmd = NodeUtil::refNode(base);
        m_director->sendCmd(ENUM_DIR_SENDER, refCmd);
    }
}

bool Receiver::exists(int fd) const {
    return m_center->exists(fd);
}

EnumSockRet Receiver::recvTcp(GenData* data, 
    int max, int* prdLen) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int fd = m_center->getFd(data);
    int rdLen = 0;
    int size = 0;
    int errcode = 0;
    int total = 0; 

    if (!(0 < max) || max > MAX_SIZE_ONCE_RDWR) {
        max = MAX_SIZE_ONCE_RDWR;
    } 

    while (0 < max && ENUM_SOCK_RET_OK == ret) {
        if (MAX_BUFF_SIZE < max) {
            size = MAX_BUFF_SIZE;
        } else {
            size = max;
        }

        ret = SockTool::recvTcp(fd, m_buf, size, &rdLen);
        if (0 < rdLen) {
            total += rdLen;
            max -= rdLen;
            
            errcode = m_center->onRecv(data, m_buf, rdLen, NULL);
            if (0 != errcode) {
                ret = ENUM_SOCK_RET_PARSED;
            }
        }
    } 
    
    if (NULL != prdLen) {
        *prdLen = total;
    }
    
    return ret;
}

EnumSockRet Receiver::recvUdp(
    GenData* data, int max, int* prdLen) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int fd = m_center->getFd(data);
    int total = 0;
    int rdlen = 0;
    int errcode = 0;
    struct iovec iov;
    SockAddr addr; 

    if (!(0 < max) || max > MAX_SIZE_ONCE_RDWR) {
        max = MAX_SIZE_ONCE_RDWR;
    }
    
    while (ENUM_SOCK_RET_OK == ret &&
        total < max) {
        iov.iov_base = m_buf;
        iov.iov_len = sizeof(m_buf);
    
        ret = SockTool::recvVec(fd, &iov, 1, &rdlen, &addr);
        if (0 < rdlen) { 
            total += rdlen;

            errcode = m_center->onRecv(data, m_buf, rdlen, &addr); 
            if (0 != errcode) {
                ret = ENUM_SOCK_RET_PARSED;
            }
        } 
    } 

    if (NULL != prdLen) {
        *prdLen = total;
    }
    
    return ret;
}

