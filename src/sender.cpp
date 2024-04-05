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
    &Sender::writeTimer,
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
    
    m_ev_fd = 0;
    m_busy = false;
}

Sender::~Sender() {
}

int Sender::init() {
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
        m_pfds[i].events = POLLOUT;
    }  

    m_timer = new TickTimer;
    
    m_1sec_obj = TickTimer::allocObj();
    TickTimer::setTimerCb(m_1sec_obj, &Sender::Send1SecCb, (long)this);
    startTimer1Sec();

    m_pfds[0].fd = m_ev_fd;
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

    if (0 < m_ev_fd) {
        SockTool::closeSock(m_ev_fd);
        m_ev_fd = 0;
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

bool Sender::wait() {
    GenData* data = NULL;
    LList* node = NULL;
    int timeout = 1000;
    int size = 0;
    int cnt = 0;

    size = 1;
    for_each_list(node, &m_wait_queue) {
        data = CONTAINER(node, GenData, m_wr_node);
        m_pfds[size++].fd = data->m_fd;
    }

    cnt = poll(m_pfds, size, timeout);
    if (0 == cnt) {
        /* empty */
        return false;
    } else if (0 < cnt) { 
        /* index of 0 is the special event fd */
        if (0 != m_pfds[0].revents) {
            MiscTool::read8Bytes(m_ev_fd);
            
            /* reset the flag */
            m_pfds[0].revents = 0; 
            --cnt;
        }

        for (int i=1; i<size && 0 < cnt; ++i) {
            if (0 != m_pfds[i].revents) { 
                data = find(m_pfds[i].fd); 
                    
                /* move from wait queue to run queue */
                del(&data->m_wr_node);
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
    MiscTool::writeEvent(m_ev_fd);
}

void Sender::setStat(GenData* data, int stat) {
    m_center->setStat(ENUM_DIR_SENDER, data, stat); 
}

void Sender::lockSet(GenData* data, int stat) {
    lock();
    setStat(data, stat);
    unlock();
}

int Sender::_activate(GenData* data, int expectStat) {
    bool action = false;

    lock(); 
    action = _queue(data, expectStat);
    unlock();
    
    if (action) {
        signal();
    }
    
    return 0;
}

int Sender::activate(GenData* data) {
    int ret = 0;

    ret = _activate(data, ENUM_STAT_DISABLE);
    return ret;
}

bool Sender::_queue(GenData* data, int expectStat) {
    bool action = false;

    if (expectStat == data->m_wr_stat) {
        setStat(data, ENUM_STAT_READY);

        if (!m_busy) {
            m_busy = true;
            action = true;
        }
        
        add(&m_run_queue, &data->m_wr_node);
    }

    return action;
}

bool Sender::queue(GenData* data) {
    bool action = false;
    
    lock();
    action = _queue(data, ENUM_STAT_BLOCKING);
    unlock();

    return action;
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

    data = find(fd); 
    
    lock(); 
    MsgUtil::addNodeMsg(&data->m_wr_msg_que, pMsg);
    action = _queue(data, ENUM_STAT_IDLE); 
    unlock();

    if (action) {
        signal();
    }

    return 0;
}

int Sender::notifyTimer(GenData* data, unsigned tick) {
    bool action = false;
    
    lock();
    data->m_wr_tick += tick;
    action = _queue(data, ENUM_STAT_IDLE); 
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
        data = CONTAINER(node, GenData, m_wr_node); 

        del(&data->m_wr_node); 
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
}

void Sender::startTimer1Sec() {
    m_timer->schedule(m_1sec_obj, DEF_NUM_PER_SEC);
}

void Sender::dealFlashTimeout(unsigned now, LList* list) { 
    GenData* data = NULL;
    LList* node = NULL;
    LList* n = NULL;
    bool expired = false;

    for_each_list_safe(node, n, list) {
        data = CONTAINER(node, GenData, m_wr_timeout_node);

        expired = m_center->chkExpire(ENUM_DIR_SENDER, data, now);
        if (!expired) {
            break;
        } else {
            del(&data->m_wr_timeout_node); 
            m_director->notifyClose(data);
        }
    } 
}

void Sender::addFlashTimeout(unsigned now, 
    LList* list, GenData* data) {
    bool action = false;
    
    /* timeout check */
    action = m_center->updateExpire(ENUM_DIR_SENDER, data, now);
    if (action) {
        del(&data->m_wr_timeout_node);
        add(list, &data->m_wr_timeout_node);
    }
}

void Sender::callback(GenData* data) {
    if (0 <= data->m_wr_cb && ENUM_WR_END > data->m_wr_cb) {
        (this->*(m_func[data->m_wr_cb]))(data); 
    } else { 
        (this->*(m_func[ENUM_WR_DEFAULT]))(data); 
    }
}

void Sender::writeDefault(GenData* data) {
    int fd = m_center->getFd(data);
    
    LOG_INFO("fd=%d| stat=%d| cb=%d| invalid write data", 
        fd, data->m_wr_stat, data->m_wr_cb);
    
    lockSet(data, ENUM_STAT_ERROR);
    m_director->notifyClose(data);
}

void Sender::writeTimer(GenData* data) {
    unsigned tick = 0;

    lock();
    tick = data->m_wr_tick;
    data->m_wr_tick = 0; 
    setStat(data, ENUM_STAT_IDLE);
    unlock();

    if (0 < tick) {
        m_timer->run(tick);
    }
}

void Sender::writeConnector(GenData* data) {
    bool bOk = false;
    int fd = m_center->getFd(data);

    bOk = SockTool::chkConnectStat(fd);
    if (bOk) {
        data->m_wr_conn = 0;
    } else {
        data->m_wr_conn = -1; 
    } 

    lockSet(data, ENUM_STAT_DISABLE);
    m_director->activate(ENUM_DIR_DEALER, data);
}

void Sender::writeSock(GenData* data) {
    int fd = m_center->getFd(data);
    unsigned now = m_timer->now();
    int ret = 0;
    unsigned total = 0;
    unsigned max = 0;
    LList* node = NULL;
    NodeMsg* base = NULL;
    unsigned threshold = 0;
    int stat = ENUM_STAT_READY;

    m_center->clearBytes(ENUM_DIR_SENDER, data, now);
    threshold = m_center->getWrThresh(data); 
    
    if (0 < threshold) { 
        max = m_center->calcThresh(ENUM_DIR_SENDER, data, now);
        if (0 == max) {
            stat = ENUM_STAT_FLOWCTL;
            flowCtl(data, 0);
        }
    } else {
        max = 0;
    } 

    base = data->m_wr_cur_msg;
    while (ENUM_STAT_READY == stat) {
        if (NULL == base) {
            lock();
            if (!isEmpty(&data->m_wr_msg_que)) {
                node = first(&data->m_wr_msg_que);
                base = MsgUtil::toNodeMsg(node);

                del(node); 
            } else {
                setStat(data, ENUM_STAT_IDLE);
                stat = ENUM_STAT_IDLE;
            }
            unlock();
        } 

        if (NULL != base) {
            ret = m_center->writeMsg(fd, base, max); 
            if (MsgUtil::completedMsg(base)) {
                
                MsgUtil::freeNodeMsg(base);
                base = NULL;
            }

            if (0 < ret) { 
                total += ret;
                if (0 < max && total >= max) {
                    stat = ENUM_STAT_FLOWCTL;
                    flowCtl(data, total);
                }
            } else if (0 == ret) {
                stat = ENUM_STAT_BLOCKING;
                add(&m_wait_queue, &data->m_wr_node);
                lockSet(data, ENUM_STAT_BLOCKING);
            } else {
                stat = ENUM_STAT_ERROR;
                writeDefault(data);
            }
        } 
    }

    data->m_wr_cur_msg = base;

    postSendData(now, data, stat, max, total);
}

void Sender::postSendData(unsigned now, GenData* data,
    int stat, unsigned max, unsigned total) {
    
    if (0 < total) {
        m_center->recordBytes(ENUM_DIR_SENDER, data, now, total);

        addFlashTimeout(now, &m_time_flash_queue, data);

        (void)stat;
        (void)max;
        LOG_DEBUG("write_sock| fd=%d| stat=%d| max=%u| total=%u|",
            m_center->getFd(data), stat, max, total);  
    }
}

void Sender::flowCtlCallback(GenData* data) {
    LOG_DEBUG("<== end wr flowctl| fd=%d| now=%u|",
        data->m_fd, m_timer->now());
    
    _activate(data, ENUM_STAT_FLOWCTL);
}

void Sender::flowCtl(GenData* data, unsigned total) { 
    (void)total;
    LOG_DEBUG("==> begin wr flowctl| fd=%d| now=%u| total=%u|",
        data->m_fd, m_timer->now(), total);
    
    lockSet(data, ENUM_STAT_FLOWCTL); 
    m_timer->schedule(&data->m_wr_flowctl, DEF_FLOWCTL_TICK_NUM);
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
    CmdComm* pCmd = NULL;
    GenData* data = NULL;

    pCmd = MsgUtil::getCmdBody<CmdComm>(base);
    fd = pCmd->m_fd;
    
    if (exists(fd)) { 
        data = find(fd);

        if(ENUM_STAT_INVALID == data->m_wr_stat) {
            lock();
            setStat(data, ENUM_STAT_BLOCKING);
            
            /* first to wait for write */
            add(&m_wait_queue, &data->m_wr_node);
            unlock();

            addFlashTimeout(m_timer->now(), 
                &m_time_flash_queue, data);
        }
    }
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

        lock(); 
        setStat(data, ENUM_STAT_INVALID); 
        del(&data->m_wr_node); 
        unlock();

        del(&data->m_wr_timeout_node);
        TickTimer::delTimer(&data->m_wr_flowctl); 
        
        /* go to the third step of closing */
        refCmd = MsgUtil::refNodeCmd(base);
        m_director->sendCmd(ENUM_DIR_DEALER, refCmd);
    }
}

bool Sender::exists(int fd) const {
    return m_center->exists(fd);
}

