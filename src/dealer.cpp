#include<cstdlib>
#include<cstring>
#include"shareheader.h"
#include"dealer.h"
#include"msgutil.h"
#include"director.h"
#include"managecenter.h"
#include"ticktimer.h"
#include"lock.h"
#include"misc.h"


Dealer::PDealFunc Dealer::m_func[ENUM_DEAL_END] = {
    &Dealer::procDefault,
    &Dealer::procMsg,
    &Dealer::procConnector,
    &Dealer::procListener,
    &Dealer::procTimer,
};

Dealer::Dealer(ManageCenter* center, Director* director) {
    m_center = center;
    m_director = director;
    m_timer = NULL;
    m_lock = NULL;
    
    initList(&m_run_queue);
    initList(&m_cmd_queue);

    m_busy = false;
}

Dealer::~Dealer() {
}

int Dealer::init() {
    int ret = 0;

    m_lock = new MutexCond; 
    m_timer = new TickTimer;
    
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

    if (!action) {
        m_busy = false;
        wait();
    }
    unlock();

    if (action) {
        if (!isEmpty(&runlist)) { 
            dealRunQue(&runlist);
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

void Dealer::lockSet(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    unlock();
}

int Dealer::notifyTimer(GenData* data, unsigned tick) {
    bool action = false;
    
    lock();
    data->m_deal_tick += tick;
    action = _queue(data, ENUM_STAT_IDLE); 
    if (action) {
        signal();
    } 
    
    unlock();

    return 0;
}

int Dealer::addCmd(NodeCmd* pCmd) {
    lock(); 
    MsgUtil::addNodeCmd(&m_cmd_queue, pCmd);
    if (!m_busy) {
        m_busy = true;
        signal();
    }
    
    unlock(); 
    return 0;
}

int Dealer::_activate(GenData* data, int expectStat) {
    bool action = false;

    lock(); 
    action = _queue(data, expectStat); 
    if (action) {
        signal();
    }
    
    unlock(); 
    return 0;
}

int Dealer::activate(GenData* data) {
    int ret = 0;

    ret = _activate(data, ENUM_STAT_DISABLE);
    return ret;
}

bool Dealer::_queue(GenData* data, int expectStat) {
    bool action = false;

    if (expectStat == data->m_deal_stat) {
        data->m_deal_stat = ENUM_STAT_READY;

        if (!m_busy) {
            m_busy = true;
            action = true;
        }
        
        add(&m_run_queue, &data->m_deal_node);
    }

    return action;
}

int Dealer::dispatch(int fd, NodeMsg* pMsg) {
    bool action = false;
    GenData* data = find(fd); 
    
    lock(); 
    MsgUtil::addNodeMsg(&data->m_deal_msg_que, pMsg);
    action = _queue(data, ENUM_STAT_IDLE);
    if (action) {
        signal();
    }
    
    unlock(); 
    return 0;
}

void Dealer::dealCmds(LList* list) {
    LList* n = NULL;
    LList* node = NULL;
    NodeCmd* base = NULL;

    for_each_list_safe(node, n, list) {
        base = MsgUtil::toNodeCmd(node); 
        del(node);

        procCmd(base);
        MsgUtil::freeNodeCmd(base);
    }
}

void Dealer::dealRunQue(LList* list) {
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;

    for_each_list_safe(node, n, list) {
        data = CONTAINER(node, GenData, m_deal_node); 

        del(&data->m_deal_node); 
        callback(data);
    }
}

void Dealer::callback(GenData* data) {
    if (0 <= data->m_deal_cb && ENUM_DEAL_END > data->m_deal_cb) {
        (this->*(m_func[data->m_deal_cb]))(data); 
    } else {
        (this->*(m_func[ENUM_DEAL_DEFAULT]))(data); 
    }
}

void Dealer::procDefault(GenData* data) {
    int fd = data->m_fd;

    LOG_INFO("fd=%d| stat=%d| cb=%d| invalid deal data", 
        fd, data->m_deal_stat, data->m_deal_cb);
    
    lockSet(data, ENUM_STAT_ERROR);
    
    m_director->notifyClose(data);
}

void Dealer::procTimer(GenData* data) {
    unsigned tick = 0;

    lock();
    tick = data->m_deal_tick;
    data->m_deal_tick = 0; 
    setStat(data, ENUM_STAT_IDLE);
    unlock();

    if (0 < tick) {
        m_timer->run(tick);
    }
}

void Dealer::procMsg(GenData* data) { 
    int ret = 0;
    NodeMsg* base = NULL;
    LList* node = NULL;
    LList runlist = INIT_LIST(runlist); 

    lock(); 
    if (!isEmpty(&data->m_deal_msg_que)) {
        append(&runlist, &data->m_deal_msg_que);
    }
    
    setStat(data, ENUM_STAT_IDLE);
    unlock();

    if (!isEmpty(&runlist)) {
        while (!isEmpty(&runlist)) {
            node = first(&runlist);
            base = MsgUtil::toNodeMsg(node);
            del(node);

            if (0 == ret) {
                ret = m_director->onProcess(data, base);
            }
            
            MsgUtil::freeNodeMsg(base);
        }

        if (0 != ret) {
            procDefault(data);
        }
    }
}

void Dealer::procCmd(NodeCmd* pCmd) { 
    CmdHead_t* pHead = NULL;

    pHead = MsgUtil::getCmd(pCmd);
    switch (pHead->m_cmd) {
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
    int result = data->m_wr_conn;

    m_director->onConnect(data, result);
}

void Dealer::procListener(GenData* listenData) {
    int listenFd = m_center->getFd(listenData);
    int ret = 0; 
    int newFd = -1;
    SockParam param;

    do {
        ret = SockTool::acceptSock(listenFd, &newFd, &param); 
        if (0 < ret) {
            m_director->onAccept(listenData, newFd, 
                param.m_ip, param.m_port); 
        } 
    } while (0 < ret);

    if (0 <= ret) {
        lockSet(listenData, ENUM_STAT_DISABLE);
        m_director->activate(ENUM_DIR_RECVER, listenData);
    } else { 
        procDefault(listenData);
    }
}

void Dealer::cmdRemoveFd(NodeCmd* base) { 
    int fd = -1;
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;

    LOG_INFO("dealer_remove_fd| fd=%d|", fd);
    
    if (exists(fd)) { 
        data = find(fd);

        lock();
        setStat(data, ENUM_STAT_INVALID);
        
        /* delete from run que if in */
        del(&data->m_deal_node); 
        unlock();

        m_director->onClose(data);
    }
}

void Dealer::cmdSchedTask(NodeCmd* base) { 
    CmdSchedTask* pCmd = NULL;
    TimerObj* obj = NULL;

    pCmd = MsgUtil::getCmdBody<CmdSchedTask>(base);

    if (0 < pCmd->m_delay) {
        obj = TickTimer::allocObj();
        if (NULL != obj) {
            TickTimer::setTimerCb(obj, pCmd->func, 
                pCmd->m_data, pCmd->m_data2);

            m_timer->schedule(obj, pCmd->m_delay);
        }
    } else {
        /* instant task */
        pCmd->func(pCmd->m_data, pCmd->m_data2);
    }
}

