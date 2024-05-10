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


bool Dealer::dealSecCb(long data1, long, TimerObj*) {
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
    m_1sec_obj = NULL;
    
    initList(&m_run_queue);
    initList(&m_cmd_queue);

    m_busy = false;
    m_tick = 0;
}

Dealer::~Dealer() {
}

int Dealer::init() {
    int ret = 0;

    m_lock = new MutexCond; 
    m_timer = new TickTimer;
    
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, &Dealer::dealSecCb, (long)this);
    startTimer1Sec();
    
    return ret;
}

void Dealer::finish() {
    if (NULL != m_1sec_obj) {
        TickTimer::freeObj(m_1sec_obj);
        m_1sec_obj = NULL;
    }
    
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
}

void Dealer::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, 0, DEF_NUM_PER_SEC);
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
    bool action = false;

    lock();
    if (!isEmpty(&m_run_queue)) {
        append(&runlist, &m_run_queue);
        action = true;
    } 

    if (!isEmpty(&m_cmd_queue)) {
        append(&cmdlist, &m_cmd_queue);
        action = true;
    }

    if (0 < m_tick) {
        tick = m_tick;
        m_tick = 0;
        action = true;
    }

    if (!action) {
        m_busy = false;
        
        if (isRun()) {
            wait();
        }
    }
    unlock();

    if (action) {
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
}

int Dealer::requeue(GenData* data) {
    bool action = false;

    lock();
    action = _queue(data, ENUM_STAT_READY); 
    if (action) {
        signal();
    }
    unlock();
    
    return 0;
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
    bool action = false;
    GenData* data = find(fd);

    LOG_DEBUG("fd=%d| msg=dispatch msg|", fd);

    if (!m_center->isClosed(data)) {
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
    
    detach(data, ENUM_STAT_ERROR); 
    
    m_director->notifyClose(data);
}

void Dealer::procMsg(GenData* data) { 
    int ret = 0;
    NodeMsg* base = NULL;
    LList* node = NULL;
    LList runlist = INIT_LIST(runlist); 

    lock(); 
    m_center->appendQue(ENUM_DIR_DEALER, &runlist, data);
    
    if (isEmpty(&runlist)) {
        setStat(data, ENUM_STAT_IDLE);
    }
    unlock();

    if (!isEmpty(&runlist)) {
        while (!isEmpty(&runlist)) {
            node = first(&runlist);
            base = NodeUtil::toNode(node);
            del(node);

            if (0 == ret) {
                ret = m_center->onProcess(data, base);
            }
            
            NodeUtil::freeNode(base);
        }

        if (0 == ret) {
            requeue(data);
        } else {
            LOG_INFO("proc_msgs| fd=%d| ret=%d| msg=deal close|", 
                m_center->getFd(data), ret);
            
            detach(data, ENUM_STAT_ERROR); 
    
            m_director->notifyClose(data);
        }
    }
}

void Dealer::procCmd(NodeMsg* pCmd) { 
    int cmd = 0;

    cmd = CmdUtil::getCmd(pCmd);
    switch (cmd) { 
    case ENUM_CMD_REMOVE_FD:
        cmdRemoveFd(pCmd);
        break;

    case ENUM_CMD_SCHED_TASK:
        cmdSchedTask(pCmd);
        break;

    default:
        break;
    }
}

void Dealer::procConnector(GenData* data) {
    int fd = m_center->getFd(data);
    int ret = 0;
    ConnOption opt;

    memset(&opt, 0, sizeof(opt));
    ret = m_center->onConnect(data, opt);
    if (0 == ret) {
        m_center->initSock(data); 
        
        m_director->setSpeed(fd, opt.m_rd_thresh, opt.m_wr_thresh); 
        m_director->activateSock(data, opt.m_delay);
         
    } else {
        m_director->closeData(data);
    } 
}

void Dealer::onAccept(GenData* listenData,
    int newFd, const char ip[], int port) {
    GenData* childData = NULL;
    int ret = 0;
    AccptOption opt;

    childData = m_center->reg(newFd);
    if (NULL != childData) {
        m_center->setAddr(childData, ip, port); 
        
        memset(&opt, 0, sizeof(opt));
        ret = m_center->onNewSock(listenData, childData, opt);
        if (0 == ret) { 
            m_center->initSock(childData); 
            
            m_director->setSpeed(newFd, opt.m_rd_thresh, opt.m_wr_thresh); 
            m_director->activateSock(childData, opt.m_delay); 
        } else {
            m_director->closeData(childData);
        }
    } else {
        SockTool::closeSock(newFd);
    }
}

void Dealer::procListener(GenData* listenData) {
    int listenFd = m_center->getFd(listenData);
    int ret = 0; 
    int newFd = -1;
    SockParam param;

    do {
        ret = SockTool::acceptSock(listenFd, &newFd, &param); 
        if (0 < ret) {
            onAccept(listenData, newFd, 
                param.m_ip, param.m_port); 
        } 
    } while (0 < ret);

    if (0 <= ret) {
        setStat(listenData, ENUM_STAT_DISABLE);
        m_director->activate(ENUM_DIR_RECVER, listenData);
    } else { 
        LOG_INFO("deal_listener| fd=%d| ret=%d| msg=deal close|", 
                listenFd, ret);
        
        detach(listenData, ENUM_STAT_ERROR); 
    
        m_director->notifyClose(listenData);
    }
}

void Dealer::cmdRemoveFd(NodeMsg* base) { 
    int fd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = CmdUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;

    LOG_INFO("dealer_remove_fd| fd=%d|", fd);
    
    if (exists(fd)) { 
        data = find(fd); 

        detach(data, ENUM_STAT_INVALID); 

        m_center->onClose(data);
        m_director->closeData(data);
    }
}

static bool _sheduleDealerTask(long param1, 
    long param2, TimerObj* obj) {
    ManageCenter* center = (ManageCenter*)param1;
    NodeMsg* base = (NodeMsg*)param2;
    CmdSchedTask* body = NULL;
    TimerFunc func = NULL;
    long data = 0;
    long data2 = 0;
    bool bOk = false;
    TimerObj* ele = NULL;

    body = CmdUtil::getCmdBody<CmdSchedTask>(base);
    func = body->func;
    data = body->m_data;
    data2 = body->m_data2;

    bOk = func(data, data2);
    if (bOk && 0 < obj->m_interval) {
        return true;
    } else {
        ele = (TimerObj*)obj;
        
        center->freeTimer(ele);
        NodeUtil::freeNode(base);

        return false;
    }
}

void Dealer::cmdSchedTask(NodeMsg* base) { 
    NodeMsg* refCmd = NULL;
    CmdSchedTask* body = NULL;
    TimerObj* obj = NULL;
    unsigned delay = 0;
    unsigned interval = 0; 

    body = CmdUtil::getCmdBody<CmdSchedTask>(base);
    delay = body->m_delay;
    interval = body->m_interval;

    if (NULL != body->func) {
        obj = m_center->allocTimer();

        if (NULL != obj) {
            refCmd = NodeUtil::refNode(base);
            TickTimer::setTimerCb(obj, &_sheduleDealerTask, 
                (long)m_center, (long)refCmd);
  
            m_timer->schedule(obj, delay, interval); 
        }
    } 
}

