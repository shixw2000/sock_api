#include<cstdlib>
#include<cstring>
#include"shareheader.h"
#include"dealer.h"
#include"msgtool.h"
#include"director.h"
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"
#include"misc.h"
#include"socktool.h"
#include"cmdcache.h"
#include"nodebase.h"
#include"isockapi.h"


bool Dealer::dealerTimeoutCb(long p1, long p2) {
    Dealer* dealer = (Dealer*)p1;
    GenData* data = (GenData*)p2;
    
    dealer->dealTimeoutCb(data); 
    return false;
}

bool Dealer::dealSecCb(long data1, long) {
    Dealer* dealer = (Dealer*)data1;

    dealer->cbTimer1Sec();
    return true;
}

Dealer::PDealFunc Dealer::m_func[ENUM_DEAL_END] = {
    &Dealer::procDefault,
    &Dealer::procMsg,
    &Dealer::procConnector,
    &Dealer::procListener,
};

Dealer::Dealer(ManageCenter* center, Director* director) {
    m_center = center;
    m_director = director;
    m_timer = NULL;
    m_lock = NULL;
    
    initList(&m_run_queue);
    initList(&m_run_queue_priv);
    initList(&m_cmd_queue);

    TickTimer::initObj(&m_1sec_obj);
    
    m_busy = false;
    m_tick = 0;
}

Dealer::~Dealer() {
}

int Dealer::init() {
    int ret = 0;

    m_lock = new MutexCond; 
    m_timer = new TickTimer;
    
    TickTimer::setTimerCb(&m_1sec_obj, &Dealer::dealSecCb, (long)this);
    startTimer1Sec();
    
    return ret;
}

void Dealer::finish() { 
    if (NULL != m_timer) {
        delete m_timer;
        m_timer = NULL;
    }
    
    if (NULL != m_lock) {
        delete m_lock;
        m_lock = NULL;
    }
}

void Dealer::stop() {
    CThread::stop();

    lock();
    signal();
    unlock();
}

void Dealer::cbTimer1Sec() {
    unsigned now = m_timer->now();
    
    LOG_DEBUG("=======deal_now=%u|", now);

    m_director->runPerSec();
}

void Dealer::startTimer1Sec() {
    m_timer->startTimer(&m_1sec_obj, 0, DEF_NUM_PER_SEC);
}

GenData* Dealer::find(int fd) const {
    return m_center->find(fd);
} 

bool Dealer::exists(int fd) const {
    return m_center->exists(fd);
}

void Dealer::run() { 
    while (isRun()) {
        doTasks();
    }
}

void Dealer::doTasks() {
    unsigned tick = 0;
    LList runlist = INIT_LIST(runlist);
    LList cmdlist = INIT_LIST(cmdlist);
    bool done = false;

    if (!isEmpty(&m_run_queue_priv)) {
        append(&runlist, &m_run_queue_priv);
        done = true;
    } 
    
    lock();
    if (!isEmpty(&m_run_queue)) {
        append(&runlist, &m_run_queue);
        if (!done) {
            done = true;
        }
    } 

    if (!isEmpty(&m_cmd_queue)) {
        append(&cmdlist, &m_cmd_queue);
        if (!done) {
            done = true;
        }
    }

    if (0 < m_tick) {
        tick = m_tick;
        m_tick = 0;
        if (!done) {
            done = true;
        }
    } 

    if (!done) {
        if (m_busy) {
            m_busy = false;
        }
        
        wait();
    }
    unlock();

    if (done) {
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
}

void Dealer::lock() {
    m_lock->lock();
}

void Dealer::unlock() {
    m_lock->unlock();
}

void Dealer::signal() {
    m_lock->signal();
}

void Dealer::wait() {
    m_lock->wait();
}

void Dealer::setStat(GenData* data, int stat) {
    m_center->setStat(ENUM_DIR_DEALER, data, stat); 
}

int Dealer::notifyTimer(unsigned tick) {
    lock();
    m_tick += tick;
    if (!m_busy) {
        m_busy = true;
        signal();
    } 
    
    unlock();

    return 0;
} 

int Dealer::addCmd(NodeMsg* pCmd) {
    lock(); 
    NodeUtil::queue(&m_cmd_queue, pCmd);
    if (!m_busy) {
        m_busy = true;
        signal();
    }
    
    unlock(); 
    return 0;
}

int Dealer::activate(GenData* data) {
    bool action = false;

    lock(); 
    action = _queue(data, ENUM_STAT_DISABLE); 
    if (action) {
        signal();
    }
    
    unlock(); 
    return 0;
}

void Dealer::detach(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    m_center->delNode(ENUM_DIR_DEALER, data); 
    unlock();

    m_center->cancelTimer(ENUM_DIR_DEALER, m_timer, data);
}

bool Dealer::_queue(GenData* data, int expectStat) {
    bool action = false;
    int stat = m_center->getStat(ENUM_DIR_DEALER, data);

    if (expectStat == stat) {
        setStat(data, ENUM_STAT_READY);

        if (!m_busy) {
            m_busy = true;
            action = true;
        }
        
        m_center->addNode(ENUM_DIR_DEALER, &m_run_queue, data);
    }

    return action;
}

int Dealer::dispatch(int fd, NodeMsg* pMsg) {
    GenData* data = NULL;
    int stat = 0;
    bool action = false;

    LOG_DEBUG("fd=%d| msg=dispatch msg|", fd);

    if (exists(fd)) {
        data = find(fd);
        stat = m_center->getStat(ENUM_DIR_DEALER, data);
        
        if (ENUM_STAT_INVALID != stat) { 
            lock(); 
            m_center->addMsg(ENUM_DIR_DEALER, data, pMsg);
            action = _queue(data, ENUM_STAT_IDLE);
            if (action) {
                signal();
            }
            
            unlock(); 
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

void Dealer::dealCmds(LList* list) {
    LList* n = NULL;
    LList* node = NULL;
    NodeMsg* base = NULL;

    for_each_list_safe(node, n, list) {
        base = NodeUtil::toNode(node); 
        del(node);

        procCmd(base);
        NodeUtil::freeNode(base);
    }
}

void Dealer::dealRunQue(LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        data = m_center->fromNode(ENUM_DIR_DEALER, node);

        m_center->delNode(ENUM_DIR_DEALER, data);
        callback(data);
    }
}

void Dealer::callback(GenData* data) {
    int cb = m_center->getCb(ENUM_DIR_DEALER, data);
    
    if (0 <= cb && ENUM_DEAL_END > cb) {
        (this->*(m_func[cb]))(data); 
    } else {
        (this->*(m_func[ENUM_DEAL_DEFAULT]))(data); 
    }
}

void Dealer::procDefault(GenData* data) {
    LOG_INFO("fd=%d| stat=%d| cb=%d| msg=invalid deal cb|", 
        m_center->getFd(data), 
        m_center->getStat(ENUM_DIR_DEALER, data),
        m_center->getCb(ENUM_DIR_DEALER, data));
    
    detach(data, ENUM_STAT_INVALID); 
    
    m_director->stopSock(data);
}

void Dealer::procMsg(GenData* data) { 
    int ret = 0;
    LList* list = NULL;

    lock(); 
    list = m_center->getDealQue(data); 
    if (isEmpty(list)) {
        setStat(data, ENUM_STAT_IDLE);
    }
    unlock();

    if (!isEmpty(list)) { 
        ret = dealMsgQue(data, list); 
        if (0 == ret) {
            m_center->addNode(ENUM_DIR_DEALER,
                &m_run_queue_priv, data);

            if (!m_busy) {
                m_busy = true;
            }
        } else { 
            detach(data, ENUM_STAT_INVALID); 
            m_director->stopSock(data);
        } 
    }
}

int Dealer::dealMsgQue(GenData* data, LList* list) {
    LList* node = NULL;
    LList* n = NULL;
    NodeMsg* pMsg = NULL;
    int ret = 0;
    
    for_each_list_safe(node, n, list) {
        pMsg = NodeUtil::toNode(node);

        if (0 == ret && isRun()) {
            if (!CmdUtil::isSockExit(pMsg)) {
                ret = m_center->onProcess(data, pMsg);
            } else {
                /* receive exit msg */
                ret = 1;
                break;
            }
        } else {
            break;
        }

        del(node);
        NodeUtil::freeNode(pMsg);
    }

    return ret;
} 

void Dealer::procCmd(NodeMsg* pCmd) { 
    int cmd = 0;

    cmd = CmdUtil::getCmd(pCmd);
    switch (cmd) { 
    case ENUM_CMD_REMOVE_FD:
        cmdRemoveFd(pCmd);
        break;

    default:
        break;
    }
} 

void Dealer::procConnector(GenData* data) {
    int ret = 0;
    ConnOption opt;

    MiscTool::bzero(&opt, sizeof(opt));
    
    m_director->beginSock(data);
    ret = m_center->onConnect(data, opt);
    if (0 == ret) {
        m_director->activateSock(data, opt.m_rd_thresh,
            opt.m_wr_thresh, opt.m_delay); 
    } else {
        closeData(data);
    } 
}

void Dealer::closeData(GenData* data) {
    int fd = m_center->getFd(data);
    bool bOk = false;
    
    bOk = m_center->unreg(data); 
    if (bOk) {
        SockTool::closeSock(fd);
    }
}

void Dealer::onAccept(GenData* listenData,
    int newFd, const SockAddr* addr) {
    int ret = 0; 

    ret = m_director->regSock(newFd, listenData, addr);
    if (0 != ret) {
        SockTool::closeSock(newFd);
    } 
}

void Dealer::procListener(GenData* listenData) {
    int listenFd = m_center->getFd(listenData);
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int newFd = -1;
    SockAddr addr; 

    ret = SockTool::acceptSock(listenFd, &newFd, &addr); 
    while (ENUM_SOCK_RET_OK == ret) {
        onAccept(listenData, newFd, &addr);
        
        ret = SockTool::acceptSock(listenFd, &newFd, &addr); 
    }

    if (ENUM_SOCK_RET_BLOCK == ret) {
        /* go to receiver to read */
        setStat(listenData, ENUM_STAT_DISABLE);
        m_director->activate(ENUM_DIR_RECVER, listenData);
    } else if (ENUM_SOCK_RET_LIMIT == ret) {
        /* sleep to wait */
        m_center->restartTimer(ENUM_DIR_DEALER, m_timer, listenData,
            ENUM_TIMER_EVENT_LISTENER_SLEEP,
            DEF_LISTENER_SLEEP_TICK);
    } else { 
        LOG_WARN("deal_listener| fd=%d| ret=%d| msg=deal close|", 
                listenFd, ret);
        
        detach(listenData, ENUM_STAT_INVALID); 
    
        m_director->stopSock(listenData);
    }
} 

void Dealer::cmdRemoveFd(NodeMsg* base) { 
    CmdComm* pCmd = NULL;
    GenData* data = NULL;
    int fd = 0;
    int port = 0;
    char ip[DEF_IP_SIZE] = {0};

    pCmd = CmdUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;

    m_center->getAddr(fd, &port, ip, sizeof(ip));

    LOG_INFO("dealer_remove_fd| fd=%d| ip=%s:%d|", 
        fd, ip, port);
    
    if (exists(fd)) { 
        data = find(fd); 

        detach(data, ENUM_STAT_INVALID); 

        m_center->onClose(data);
        closeData(data);
    }
}

void Dealer::dealTimeoutCb(GenData* data) {
    int event = 0;

    event = ManageCenter::getEvent(ENUM_DIR_DEALER, data); 
    switch (event) {
    case ENUM_TIMER_EVENT_LISTENER_SLEEP:
        procListener(data);
        break;
    default:
        break;
    }
}


