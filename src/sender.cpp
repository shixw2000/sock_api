#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"sender.h"
#include"msgutil.h"
#include"misc.h"
#include"director.h"
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"


void Sender::Send1SecCb(long data1, long) {
    Sender* sender = (Sender*)data1;

    sender->cbTimer1Sec();
}

Sender::PWrFunc Sender::m_func[ENUM_WR_END] = {
    &Sender::writeDefault,
    &Sender::writeSock,
    &Sender::writeConnector,
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
    initList(&m_cmd_queue);
    initList(&m_time_flash_queue);
    initList(&m_time_flash_conn);
    
    m_ev_fd[0] = m_ev_fd[1] = 0;
    m_busy = false;
    m_tick = 0;
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

    m_pfds = (struct pollfd*)calloc(cap, sizeof(struct pollfd));
    for (int i=0; i<cap; ++i) {
        m_pfds[i].events = POLLOUT;
    }  

    m_timer = new TickTimer;
    
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, &Sender::Send1SecCb, (long)this);
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
    wait(); 
    consume(); 
} 

void Sender::consume() {
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
        m_timer->run(tick);
    }

    if (!isEmpty(&cmdlist)) { 
        dealCmds(&cmdlist);
    } 
}

bool Sender::wait() {
    GenData* data = NULL;
    LList* node = NULL;
    int timeout = DEF_POLL_TIME_MSEC;
    int size = 0;
    int cnt = 0;

    size = 1;
    for_each_list(node, &m_wait_queue) {
        data = m_center->fromNode(ENUM_DIR_SENDER, node);
        m_pfds[size++].fd = m_center->getFd(data);
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

        for (int i=1; i<size && 0 < cnt; ++i) {
            if (0 != m_pfds[i].revents) { 
                data = find(m_pfds[i].fd); 
                    
                /* move from wait queue to run queue */
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

bool Sender::queue(GenData* data) {
    bool action = false;
    
    /* delete from wait queue, no need lock */
    m_center->delNode(ENUM_DIR_SENDER, data);

    /* add to run queue */
    lock();
    action = _queue(data, ENUM_STAT_BLOCKING);
    unlock();
    
    return action;
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

int Sender::addCmd(NodeCmd* pCmd) {
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

int Sender::sendMsg(int fd, NodeMsg* pMsg) {
    bool action = false;
    GenData* data = NULL; 

    LOG_DEBUG("fd=%d| msg=send msg|", fd);

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
        MsgUtil::freeNodeMsg(pMsg);
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
    unsigned now = m_timer->now();
    
    LOG_DEBUG("=======sender_now=%u|", now);
    
    startTimer1Sec();
    
    /* every one second */
    if (!isEmpty(&m_time_flash_queue)) {
        dealFlashTimeout(now, &m_time_flash_queue);
    }

    /* checkk connect timeout */
    if (!isEmpty(&m_time_flash_conn)) {
        dealFlashConn(now, &m_time_flash_conn);
    }
}

void Sender::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, DEF_NUM_PER_SEC);
}

void Sender::dealFlashConn(unsigned now, LList* list) { 
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;
    bool expired = false;

    for_each_list_safe(node, n, list) {
        data = m_center->fromTimeout(ENUM_DIR_SENDER, node);

        expired = m_center->chkExpire(ENUM_DIR_SENDER, data, now);
        if (!expired) {
            break;
        } else {
            detach(data, ENUM_STAT_TIMEOUT); 

            /* connect timeout */
            m_center->setConnRes(data, -2);
            m_director->activate(ENUM_DIR_DEALER, data);
        }
    } 
}

void Sender::dealFlashTimeout(unsigned now, LList* list) { 
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;
    bool expired = false;

    for_each_list_safe(node, n, list) {
        data = m_center->fromTimeout(ENUM_DIR_SENDER, node);

        expired = m_center->chkExpire(ENUM_DIR_SENDER, data, now);
        if (!expired) {
            break;
        } else {
            LOG_INFO("flash_timeout| fd=%d| msg=write close|", 
                m_center->getFd(data));
        
            detach(data, ENUM_STAT_TIMEOUT); 
     
            m_director->notifyClose(data);
        }
    } 
}

void Sender::addFlash(unsigned now, 
    LList* list, GenData* data) {
    bool action = false;
    
    /* timeout check */
    action = m_center->updateExpire(ENUM_DIR_SENDER, data, now);
    if (action) {
        m_center->delTimeout(ENUM_DIR_SENDER, data);
        m_center->addTimeout(ENUM_DIR_SENDER, list, data);
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

    setStat(data, ENUM_STAT_DISABLE);

    /* delete from timeout */
    m_center->delTimeout(ENUM_DIR_SENDER, data);
    m_director->activate(ENUM_DIR_DEALER, data);
}

void Sender::writeSock(GenData* data) {
    unsigned now = m_timer->now();
    unsigned max = 0;
    LList* list = NULL;
    unsigned threshold = 0;

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

    list = m_center->getWrPrivQue(data);
    if (isEmpty(list)) {
        lock();
        m_center->appendQue(ENUM_DIR_SENDER, list, data);
        if (isEmpty(list)) {
            setStat(data, ENUM_STAT_IDLE);
        }
        unlock();

        if (isEmpty(list)) {
            return;
        } 
    }

    writeMsgQue(data, list, now, max);
}

void Sender::writeMsgQue(GenData* data, 
    LList* queue, unsigned now, unsigned max) {
    int fd = m_center->getFd(data);
    int ret = 0;
    unsigned total = 0;
    LList* node = NULL;
    NodeMsg* base = NULL; 
    
    while (!isEmpty(queue)) { 
        if (NULL == node) {
            node = first(queue);
            base = MsgUtil::toNodeMsg(node);
        }
        
        ret = m_center->writeMsg(fd, base, max); 
        if (MsgUtil::completedMsg(base)) {
            
            del(node);
            node = NULL;
            
            MsgUtil::freeNodeMsg(base); 
        }

        if (0 < ret) { 
            total += ret;
            if (0 < max && total >= max) {
                break;
            }
        } else {
            break;
        }
    }

    if (0 <= ret) {
        if (0 < total) {
            LOG_DEBUG("write_sock| fd=%d| ret=%d|"
                " max=%u| total=%u|",
                fd, ret, max, total);

            m_center->recordBytes(ENUM_DIR_SENDER, 
                data, now, total);
            addFlash(now, &m_time_flash_queue, data);
        }

        if (0 < ret) {
            if (isEmpty(queue)) {
                _activate(data, ENUM_STAT_READY);
            } else {
                flowCtl(data, total);
            }
        } else {
            setStat(data, ENUM_STAT_BLOCKING);
            m_center->addNode(ENUM_DIR_SENDER, &m_wait_queue, data); 
        }
    } else {
        LOG_INFO("write_sock| fd=%d| ret=%d|"
            " max=%u| total=%u| msg=close|",
            fd, ret, max, total);
        
        detach(data, ENUM_STAT_ERROR);
    
        m_director->notifyClose(data);
    }
}

void Sender::flowCtlCallback(GenData* data) {
    LOG_DEBUG("<== end wr flowctl| fd=%d| now=%u|",
        m_center->getFd(data), m_timer->now());

    _activate(data, ENUM_STAT_FLOWCTL);
}

void Sender::flowCtl(GenData* data, unsigned total) { 
    (void)total;
    LOG_DEBUG("==> begin wr flowctl| fd=%d| now=%u| total=%u|",
        m_center->getFd(data), m_timer->now(), total);
    
    setStat(data, ENUM_STAT_FLOWCTL); 
    m_center->flowctl(ENUM_DIR_SENDER, m_timer, data);
}

void Sender::dealCmds(LList* list) {
    NodeCmd* base = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        base = MsgUtil::toNodeCmd(node); 
        del(node);

        procCmd(base);
        MsgUtil::freeNodeCmd(base);
    }
}

void Sender::procCmd(NodeCmd* base) { 
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

void Sender::cmdAddFd(NodeCmd* base) {
    int fd = -1;
    int stat = 0;
    int cb = 0;
    unsigned now = m_timer->now();
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;
    
    if (exists(fd)) { 
        data = find(fd);

        cb = m_center->getCb(ENUM_DIR_SENDER, data);
        stat = m_center->getStat(ENUM_DIR_SENDER, data);

        if(ENUM_STAT_INIT == stat) {
            setStat(data, ENUM_STAT_BLOCKING);
            
            /* first to wait for write */
            m_center->addNode(ENUM_DIR_SENDER, &m_wait_queue, data);

            if (ENUM_WR_SOCK == cb) {
                addFlash(now, &m_time_flash_queue, data);
            } else if (ENUM_WR_Connector == cb) {
                addFlash(now, &m_time_flash_conn, data);
            } else {
            }
        }
    }
}

void Sender::detach(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    m_center->delNode(ENUM_DIR_SENDER, data); 
    unlock();

    m_center->delTimeout(ENUM_DIR_SENDER, data);
    m_center->unflowctl(ENUM_DIR_SENDER, data);
}

void Sender::cmdRemoveFd(NodeCmd* base) { 
    int fd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;
    NodeCmd* refCmd = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;

    LOG_INFO("sender_remove_fd| fd=%d|", fd);
    
    if (exists(fd)) { 
        data = find(fd);

        detach(data, ENUM_STAT_INVALID);
        
        /* go to the third step of closing */
        refCmd = MsgUtil::refNodeCmd(base);
        m_director->sendCmd(ENUM_DIR_DEALER, refCmd);
    }
}

bool Sender::exists(int fd) const {
    return m_center->exists(fd);
}

