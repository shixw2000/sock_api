#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"sender.h"
#include"msgtool.h"
#include"misc.h"
#include"director.h"
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"
#include"socktool.h"
#include"cmdcache.h"
#include"nodebase.h"
#include"cache.h"


bool Sender::sendSecCb(long data1, long, TimerObj*) {
    Sender* sender = (Sender*)data1;

    sender->cbTimer1Sec();
    return true;
}

bool Sender::sendTimeoutCb(long p1, 
    long p2, TimerObj*) {
    Sender* sender = (Sender*)p1;
    GenData* data = (GenData*)p2;

    sender->dealTimeoutCb(data); 
    return false;
}


Sender::PWrFunc Sender::m_func[ENUM_WR_END] = {
    &Sender::writeDefault,
    &Sender::writeSock,
    &Sender::writeConnector,
    &Sender::writeUdp,
};

Sender::Sender(ManageCenter* center, Director* director) {
    m_center = center;
    m_director = director;
    m_timer = NULL;
    m_lock = NULL; 
    m_pfds = NULL;
    m_1sec_obj = NULL;

    initList(&m_wait_queue);
    initList(&m_run_queue);
    initList(&m_priv_run_queue);
    initList(&m_cmd_queue);
    
    m_ev_fd[0] = m_ev_fd[1] = 0;
    m_busy = false;
    m_tick = 0;
    m_now_sec = 0;
}

Sender::~Sender() {
}

int Sender::init() {
    int ret = 0;
    int cap = m_center->capacity();

    ret = MiscTool::creatPipes(&m_ev_fd);
    if (0 != ret) {
        return -1;
    }

    m_lock = new SpinLock; 

    m_pfds = (struct pollfd*)CacheUtil::callocAlign(
        cap, sizeof(struct pollfd));
    for (int i=0; i<cap; ++i) {
        m_pfds[i].events = POLLOUT;
    }  

    m_timer = new TickTimer;
    
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, &Sender::sendSecCb, (long)this);
    startTimer1Sec();

    m_pfds[0].fd = m_ev_fd[0];
    m_pfds[0].events = POLLIN;

    return ret;
}

void Sender::finish() {
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
}

GenData* Sender::find(int fd) const {
    return m_center->find(fd);
} 

void Sender::run() { 
    while (isRun()) {
        doTasks();
    }
}

void Sender::doTasks() { 
    LList runlist = INIT_LIST(runlist);
    
    wait(&runlist); 
    consume(&runlist); 
} 

void Sender::consume(LList* runlist) {
    LList cmdlist = INIT_LIST(cmdlist);
    unsigned tick = 0; 

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

    if (!isEmpty(&m_priv_run_queue)) {
        append(runlist, &m_priv_run_queue);
    }
    
    if (!isEmpty(runlist)) { 
        dealRunQue(runlist);
    } 

    if (0 < tick) {
        m_timer->run(tick);
    }

    if (!isEmpty(&cmdlist)) { 
        dealCmds(&cmdlist);
    } 
}

bool Sender::wait(LList* runlist) {
    GenData* data = NULL;
    LList* node = NULL;
    int timeout = DEF_POLL_TIME_MSEC;
    int size = 0;
    int cnt = 0; 

    /* position 0 is event */
    size = 1;
    for_each_list(node, &m_wait_queue) {
        data = m_center->fromNode(ENUM_DIR_SENDER, node);
        m_pfds[size++].fd = m_center->getFd(data);
    }

    if (m_busy) {
        timeout = 0;
    }

    cnt = poll(m_pfds, size, timeout); 
    if (0 == cnt) {
        /* empty */
        return false;
    } else if (0 < cnt) { 
        /* index of 0 is the special event fd */
        if (0 != m_pfds[0].revents) {
            MiscTool::readPipeEvent(m_ev_fd[0]);
            
            /* reset the flag */
            m_pfds[0].revents = 0; 
            --cnt;
        }

        for (int i=MIN_SND_PFD; i<size && 0 < cnt; ++i) {
            if (0 != m_pfds[i].revents) { 
                data = find(m_pfds[i].fd); 
                    
                /* move from wait queue to run queue */
                toWrite(runlist, data);
                
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

void Sender::lock() {
    m_lock->lock();
}

void Sender::unlock() {
    m_lock->unlock();
}

void Sender::signal() {
    MiscTool::writePipeEvent(m_ev_fd[1]);
}

void Sender::setStat(GenData* data, int stat) {
    m_center->setStat(ENUM_DIR_SENDER, data, stat); 
}

bool Sender::_queue(GenData* data, int expectStat) {
    bool action = false;
    int stat = m_center->getStat(ENUM_DIR_SENDER, data);

    if (expectStat == stat) {
        setStat(data, ENUM_STAT_READY);

        if (!m_busy) {
            m_busy = true;
            action = true;
        }

        m_center->addNode(ENUM_DIR_SENDER, &m_run_queue, data);
    }

    return action;
}

void Sender::toWrite(LList* runlist, GenData* data) {
    /* delete from wait queue, no need lock */
    setStat(data, ENUM_STAT_READY);
    m_center->delNode(ENUM_DIR_SENDER, data);

    /* add to run queue */
    m_center->addNode(ENUM_DIR_SENDER, runlist, data); 
    if (!m_busy) {
        m_busy = true;
    }
}

int Sender::activate(GenData* data) {
    int ret = 0;

    ret = _activate(data, ENUM_STAT_DISABLE);
    return ret;
}

int Sender::_activate(GenData* data, int stat) {
    bool action = false;

    lock();
    action = _queue(data, stat); 
    unlock();
    
    if (action) {
        signal();
    }

    return 0;
}

int Sender::addCmd(NodeMsg* pCmd) {
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

int Sender::sendMsg(int fd, NodeMsg* pMsg) {
    bool action = false;
    GenData* data = NULL; 

    LOG_DEBUG("fd=%d| msg=send msg|", fd);

    if (exists(fd)) {
        data = find(fd); 
        
        if (!m_center->isClosed(data)) {
            lock(); 
            m_center->addMsg(ENUM_DIR_SENDER, data, pMsg);
            action = _queue(data, ENUM_STAT_IDLE); 
            unlock();

            if (action) {
                signal();
            }

            return 0;
        } else {
            NodeUtil::freeNode(pMsg);
            return -1;
        }
    } else {
        NodeUtil::freeNode(pMsg);
        return -1;
    }
}

int Sender::notifyTimer(unsigned tick) {
    bool action = false;
    
    lock();
    m_tick += tick;
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

void Sender::dealRunQue(LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        data = m_center->fromNode(ENUM_DIR_SENDER, node);

        m_center->delNode(ENUM_DIR_SENDER, data);
        callback(data);
    }
}

void Sender::cbTimer1Sec() {
    ++m_now_sec;
    LOG_DEBUG("=======sender_now=%u|", m_now_sec);
}

void Sender::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, 0, DEF_NUM_PER_SEC);
}

void Sender::dealConnCb(GenData* data) { 
    detach(data, ENUM_STAT_TIMEOUT); 

    /* connect timeout */
    m_center->setConnRes(data, -2);
    m_director->activate(ENUM_DIR_DEALER, data); 
}

void Sender::dealTimeoutCb(GenData* data) { 
    int event = 0;

    event = ManageCenter::getEvent(ENUM_DIR_SENDER, data);
    switch (event) { 
    case ENUM_TIMER_EVENT_TIMEOUT:
        LOG_INFO("flash_timeout| fd=%d| msg=write close|", 
            m_center->getFd(data));

        detach(data, ENUM_STAT_TIMEOUT); 
        m_director->notifyClose(data);
        break;

    case ENUM_TIMER_EVENT_CONN_TIMEOUT:
        dealConnCb(data);
        break;

    case ENUM_TIMER_EVENT_FLOWCTL:
        flowCtlCb(data);
        break;

    default:
        break;
    } 
}

void Sender::addFlashTimeout(GenData* data,
    int event, bool force) {
    bool action = false;
    
    /* timeout check */
    action = m_center->updateExpire(ENUM_DIR_SENDER, 
        data, m_now_sec, force);
    if (action) {
        m_center->cancelTimer(ENUM_DIR_SENDER, data);
        m_center->enableTimer(ENUM_DIR_SENDER, m_timer, 
            data, event);
    }
}

void Sender::callback(GenData* data) {
    int cb = m_center->getCb(ENUM_DIR_SENDER, data);
    
    if (0 <= cb && ENUM_WR_END > cb) {
        (this->*(m_func[cb]))(data); 
    } else { 
        (this->*(m_func[ENUM_WR_DEFAULT]))(data); 
    }
}

void Sender::writeDefault(GenData* data) {
    LOG_INFO("fd=%d| stat=%d| cb=%d| msg=invalid write cb|", 
        m_center->getFd(data),
        m_center->getStat(ENUM_DIR_SENDER, data), 
        m_center->getCb(ENUM_DIR_SENDER, data));
    
    detach(data, ENUM_STAT_ERROR);
    
    m_director->notifyClose(data);
}

void Sender::writeConnector(GenData* data) {
    bool bOk = false;
    int fd = m_center->getFd(data);

    bOk = SockTool::chkConnectStat(fd);
    if (bOk) {
        m_center->setConnRes(data, 0);
    } else {
        m_center->setConnRes(data, -1);
    } 

    setStat(data, ENUM_STAT_INVALID);

    /* delete from timeout */
    m_center->cancelTimer(ENUM_DIR_SENDER, data);
    m_director->activate(ENUM_DIR_DEALER, data);
}

void Sender::writeSock(GenData* data) {
    LList* wrQue = NULL;
    int fd = m_center->getFd(data);
    unsigned now = m_timer->now();
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    unsigned max = 0;
    unsigned threshold = 0;
    int total = 0; 

    m_center->clearBytes(ENUM_DIR_SENDER, data, now);
    
    threshold = m_center->getWrThresh(data); 
    if (0 < threshold) { 
        max = m_center->calcThresh(ENUM_DIR_SENDER, data, now);
        if (0 == max) {
            flowCtl(data, 0);
            return;
        }
    } else {
        max = 0;
    } 
    
    lock();
    wrQue = m_center->getWrQue(data);
    if (isEmpty(wrQue)) {
        setStat(data, ENUM_STAT_IDLE);
    }
    unlock();

    if (!isEmpty(wrQue)) {
        ret = sendTcp(fd, wrQue, max, &total);
        if (0 < total) {
            LOG_DEBUG("write_sock| fd=%d| ret=%d|"
                " max=%u| total=%d|",
                fd, ret, max, total);

            m_center->recordBytes(ENUM_DIR_SENDER, 
                data, now, total);
            
            addFlashTimeout(data, ENUM_TIMER_EVENT_TIMEOUT);
        }

        if (ENUM_SOCK_RET_OK == ret) { 
            /* for next run loop */
            m_center->addNode(ENUM_DIR_SENDER, 
                &m_priv_run_queue, data); 
            if (!m_busy) {
                m_busy = true;
            } 
        } else if (ENUM_SOCK_RET_BLOCK == ret) { 
            setStat(data, ENUM_STAT_BLOCKING);
            m_center->addNode(ENUM_DIR_SENDER, &m_wait_queue, data);
        } else { 
            detach(data, ENUM_STAT_ERROR); 
            m_director->notifyClose(data);
        }
    }
}

void Sender::writeUdp(GenData* data) {
    LList* wrQue = NULL;
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    unsigned now = m_timer->now();
    int fd = m_center->getFd(data);
    int total = 0;
    int max = 0;
    
    lock();
    wrQue = m_center->getWrQue(data);
    if (isEmpty(wrQue)) {
        setStat(data, ENUM_STAT_IDLE);
    }
    unlock();

    if (!isEmpty(wrQue)) {
        ret = sendUdp(fd, wrQue, max, &total);
        if (0 < total) {
            LOG_DEBUG("write_udp| fd=%d| ret=%d|"
                " total=%d|",
                fd, ret, total);

            m_center->recordBytes(ENUM_DIR_SENDER, 
                data, now, total);
        }
        
        if (ENUM_SOCK_RET_OK == ret) { 
            /* for next run loop */
            m_center->addNode(ENUM_DIR_SENDER, 
                &m_priv_run_queue, data); 
            if (!m_busy) {
                m_busy = true;
            } 
        } else if (ENUM_SOCK_RET_BLOCK == ret) { 
            setStat(data, ENUM_STAT_BLOCKING);
            m_center->addNode(ENUM_DIR_SENDER, &m_wait_queue, data);
        } else { 
            detach(data, ENUM_STAT_ERROR); 
            m_director->notifyClose(data);
        }
    }
}

void Sender::flowCtlCb(GenData* data) {
    LOG_DEBUG("<== end wr flowctl| fd=%d| now=%u|",
        m_center->getFd(data), m_timer->now());

    _activate(data, ENUM_STAT_FLOWCTL);

    /* switch timeout */
    addFlashTimeout(data, ENUM_TIMER_EVENT_TIMEOUT, true);
}

void Sender::flowCtl(GenData* data, unsigned total) { 
    LOG_DEBUG("==> begin wr flowctl| fd=%d| now=%u| total=%u|",
        m_center->getFd(data), m_timer->now(), total);

    m_center->cancelTimer(ENUM_DIR_SENDER, data);
    m_center->enableTimer(ENUM_DIR_SENDER, m_timer, data,
        ENUM_TIMER_EVENT_FLOWCTL,
        DEF_FLOWCTL_TICK_NUM);
    
    setStat(data, ENUM_STAT_FLOWCTL); 
}

void Sender::dealCmds(LList* list) {
    NodeMsg* base = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        base = NodeUtil::toNode(node); 
        del(node);

        procCmd(base);
        NodeUtil::freeNode(base);
    }
}

void Sender::procCmd(NodeMsg* base) { 
    int cmd = 0;

    cmd = CmdUtil::getCmd(base);
    switch (cmd) { 
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

void Sender::cmdAddFd(NodeMsg* base) {
    int fd = -1;
    int stat = 0;
    int cb = 0;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;
    
    if (exists(fd)) { 
        data = find(fd);

        cb = m_center->getCb(ENUM_DIR_SENDER, data);
        stat = m_center->getStat(ENUM_DIR_SENDER, data);

        assert(ENUM_STAT_INVALID == stat);
        
        if(ENUM_STAT_INVALID == stat) {
            setStat(data, ENUM_STAT_BLOCKING);
            
            /* first to wait for write */
            m_center->addNode(ENUM_DIR_SENDER, &m_wait_queue, data);

            if (ENUM_WR_SOCK == cb) { 
                addFlashTimeout(data, ENUM_TIMER_EVENT_TIMEOUT, true);
            } else if (ENUM_WR_Connector == cb) { 
                addFlashTimeout(data, ENUM_TIMER_EVENT_CONN_TIMEOUT, true);
            }
        }
    }
}

void Sender::detach(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    m_center->delNode(ENUM_DIR_SENDER, data); 
    unlock();

    m_center->cancelTimer(ENUM_DIR_SENDER, data);
}

void Sender::cmdRemoveFd(NodeMsg* base) { 
    int fd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;
    NodeMsg* refCmd = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;
    
    LOG_DEBUG("sender_remove_fd| fd=%d|", fd);
    
    assert(exists(fd));
    if (exists(fd)) { 
        
        data = find(fd); 
        detach(data, ENUM_STAT_INVALID);
        
        /* go to the third step of closing */
        refCmd = NodeUtil::refNode(base);
        m_director->sendCmd(ENUM_DIR_DEALER, refCmd);
    }
}

bool Sender::exists(int fd) const {
    return m_center->exists(fd);
}

EnumSockRet Sender::sendTcp(int fd,
    LList* list, int max, int* pwrLen) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int sndlen = 0;
    int total = 0;
    int cnt = 0;
    struct iovec iov[MAX_IOVEC_SIZE];
    
    if (!(0 < max) || max > MAX_SIZE_ONCE_RDWR) {
        max = MAX_SIZE_ONCE_RDWR;
    }

    while (!isEmpty(list) && 0 < max &&
        ENUM_SOCK_RET_OK == ret) { 
        cnt = prepareQue(list, iov, MAX_IOVEC_SIZE, max);

        ret = SockTool::sendVecUntil(fd, iov, cnt, &sndlen, NULL);
        if (0 < sndlen) {
            total += sndlen;
            max -= sndlen;

            purgeQue(list, sndlen);
        }
    }

    if (NULL != pwrLen) {
        *pwrLen = total;
    }
    
    return ret;
}

EnumSockRet Sender::sendUdp(int fd,
    LList* list, int max, int* pwrLen) {
    const SockAddr* preh = NULL;
    LList* node = NULL;
    NodeMsg* pMsg = NULL;
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int sndlen = 0;
    int total = 0;
    int cnt = 0;
    int size = 0;
    bool done = false;
    struct iovec iov[2];

    if (!(0 < max) || max > MAX_SIZE_ONCE_RDWR) {
        max = MAX_SIZE_ONCE_RDWR;
    }
    
    while (!isEmpty(list) && total < max &&
        ENUM_SOCK_RET_OK == ret) { 
        node = first(list);
        pMsg = NodeUtil::toNode(node);

        preh = MsgTool::getUdpAddr(pMsg);
        if (NULL != preh) {
            size = MAX_SIZE_ONCE_RDWR;
            cnt = prepareMsg(pMsg, iov, 2, &size);

            ret = SockTool::sendVec(fd, iov, cnt, &sndlen, preh);
            if (0 < sndlen) {
                total += sndlen;

                done = purgeMsg(pMsg, &sndlen);
                if (done) {
                    del(node);
                    NodeUtil::freeNode(pMsg); 
                } else {
                    /* invalid: udp must send as whole */
                    ret = ENUM_SOCK_RET_OTHER;
                }
            }
        } else {
            ret = ENUM_SOCK_RET_OTHER;
        }
    }

    if (NULL != pwrLen) {
        *pwrLen = total;
    }
    
    return ret;
}

int Sender::prepareQue(LList* list, 
    struct iovec* iov, int size, int max) {
    LList* node = NULL;
    NodeMsg* pMsg = NULL;
    int cnt = 0;
    int totalCnt = 0;

    for_each_list(node, list) { 
        if (0 < size && 0 < max) {
            pMsg = NodeUtil::toNode(node);
            
            cnt = prepareMsg(pMsg, iov, size, &max);
            if (0 < cnt) {
                totalCnt += cnt;
                iov += cnt;
                size -= cnt;
            }
        } else {
            break;
        }
    }

    return totalCnt;
}

int Sender::purgeQue(LList* list, int max) {
    LList* node = NULL;
    LList* n = NULL;
    NodeMsg* pMsg = NULL;
    bool done = false;
    int cnt = 0;
    
    for_each_list_safe(node, n, list) {
        pMsg = NodeUtil::toNode(node);

        done = purgeMsg(pMsg, &max);
        if (done) {
            del(node);
            NodeUtil::freeNode(pMsg); 
            ++cnt;
        } else {
            break;
        }
    }

    return cnt;
}

int Sender::prepareMsg(NodeMsg* pMsg,
    struct iovec* iov, int size, int* pmax) {
    char* psz = NULL;
    int left = 0;
    int pos = 0;
    int len = 0;
    int cnt = 0;

    if (cnt < size && 0 < *pmax) {
        left = MsgTool::getLeft(pMsg);
        if (0 < left) {
            pos = MsgTool::getMsgPos(pMsg);
            psz = MsgTool::getMsg(pMsg);
            len = left < *pmax ? left : *pmax; 
                
            iov[cnt].iov_base = psz + pos;
            iov[cnt].iov_len = len;
            
            ++cnt;
            *pmax -= len;
        }

        if (cnt < size && 0 < *pmax) {
            left = MsgTool::getExtraLeft(pMsg);
            if (0 < left) {
                pos = MsgTool::getExtraPos(pMsg);
                psz = MsgTool::getExtraMsg(pMsg);

                len = *pmax > left ? left : *pmax;

                iov[cnt].iov_base = psz + pos;
                iov[cnt].iov_len = len;

                ++cnt;
                *pmax -= len;
            }
        }
    } 
    
    return cnt;
}

bool Sender::purgeMsg(NodeMsg* pMsg, int* pmax) {
    int left = 0; 

    left = MsgTool::getLeft(pMsg);
    if (0 < left) {
        if (left <= *pmax) {
            MsgTool::skipMsgPos(pMsg, left);
            *pmax -= left;
        } else {
            MsgTool::skipMsgPos(pMsg, *pmax);
            *pmax = 0;
            
            return false;
        }
    }
    
    left = MsgTool::getExtraLeft(pMsg);
    if (0 < left) {
        if (left <= *pmax) {
            MsgTool::skipExtraPos(pMsg, left); 
            *pmax -= left;

            return true;
        } else {
            MsgTool::skipExtraPos(pMsg, *pmax);
            *pmax = 0;

            return false;
        }
    } else {
        return true;
    }
}

