#include<cstring>
#include<cstdlib>
#include"director.h"
#include"managecenter.h"
#include"receiver.h"
#include"sender.h"
#include"dealer.h"
#include"misc.h"
#include"misc.h"
#include"msgtool.h"
#include"ticktimer.h"
#include"socktool.h"
#include"cmdcache.h"
#include"cache.h"


Director::Director() {
    m_receiver = NULL;
    m_sender = NULL;
    m_dealer = NULL;
    m_center = NULL;

    m_fd_max = 0;
}

Director::~Director() {
}

int Director::init(int cap) {
    int ret = 0;
    unsigned long max = 0;

    ret = MiscTool::getRlimit(ENUM_RES_NOFILE, &max, NULL);
    if (0 == ret) {
        if (cap < (int)max) {
            m_fd_max = cap;
        } else {
            m_fd_max = (int)max - 1;
        }
    } else {
        m_fd_max = cap;
    }

    m_center = new ManageCenter(m_fd_max);
    ret = m_center->init();

    m_receiver = new Receiver(m_center, this);
    ret = m_receiver->init();
    
    m_sender = new Sender(m_center, this);
    ret = m_sender->init();
    
    m_dealer = new Dealer(m_center, this);
    ret = m_dealer->init();
  
    return ret;
}

void Director::finish() {
    if (NULL != m_receiver) {
        m_receiver->finish();
        
        delete m_receiver;
        m_receiver = NULL;
    }

    if (NULL != m_sender) {
        m_sender->finish();
        
        delete m_sender;
        m_sender = NULL;
    }

    if (NULL != m_dealer) {
        m_dealer->finish();
        
        delete m_dealer;
        m_dealer = NULL;
    }

    if (NULL != m_center) { 
        m_center->finish();
        
        delete m_center;
        m_center = NULL;
    } 
}

void Director::start() {
    m_dealer->start("dealer");
    m_sender->start("sender");
    m_receiver->start("receiver");
}

void Director::wait() {
    m_dealer->join();
    m_sender->join();
    m_receiver->join();
}

void Director::stop() {
    m_receiver->stop();
    m_sender->stop();
    m_dealer->stop();
}

int Director::sendCmd(EnumDir enDir, NodeMsg* pCmd) {
    int ret = 0;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        ret = m_receiver->addCmd(pCmd);
        break;

    case ENUM_DIR_SENDER:
        ret = m_sender->addCmd(pCmd);
        break;

    case ENUM_DIR_DEALER:
        ret = m_dealer->addCmd(pCmd);
        break;

    default:
        ret = -1;
        break;
    }

    return ret;
}

int Director::sendMsg(int fd, NodeMsg* pMsg) {
    int ret = 0;

    ret = m_sender->sendMsg(fd, pMsg);
    return ret;
}

int Director::dispatch(int fd, NodeMsg* pMsg) {
    int ret = 0; 
    
    ret = m_dealer->dispatch(fd, pMsg);
    return ret;
}

int Director::activate(EnumDir enDir, GenData* data) {
    int ret = 0;
    
    switch (enDir) {
    case ENUM_DIR_RECVER:
        m_receiver->activate(data);
        break;

    case ENUM_DIR_SENDER:
        m_sender->activate(data);
        break;

    case ENUM_DIR_DEALER:
        m_dealer->activate(data);
        break;
        
    default:
        ret = -1;
        break;
    }

    return ret;
}

int Director::notifyTimer(EnumDir enDir, unsigned tick) {
    int ret = 0;
    
    switch (enDir) { 
    case ENUM_DIR_SENDER:
        m_sender->notifyTimer(tick);
        break;

    case ENUM_DIR_DEALER:
        m_dealer->notifyTimer(tick);
        break;
        
    case ENUM_DIR_RECVER:
    default:
        ret = -1;
        break;
    }

    return ret;
}

int Director::sendCommCmd(EnumDir enDir, 
    EnumSockCmd cmd, int fd) {
    int ret = 0;
    NodeMsg* pCmd = NULL; 
    
    pCmd = creatCmdComm(cmd, fd);
    ret = sendCmd(enDir, pCmd);
    return ret;
}

void Director::notifyClose(GenData* data) {
    bool close = false;
    int fd = m_center->getFd(data);

    close = m_center->markClosed(data); 
    if (close) {
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_REMOVE_FD, fd);
        
        LOG_DEBUG("cmd_close_fd| fd=%d| msg=closing|", fd);
    }
}

void Director::notifyClose(int fd) {
    bool close = false;
    
    close = m_center->markClosed(fd); 
    if (close) {
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_REMOVE_FD, fd);
        
        LOG_DEBUG("cmd_close_fd| fd=%d| msg=closing|", fd);
    }
}

void Director::setTimeout(unsigned rd_timeout, unsigned wr_timeout) { 
    m_center->setTimeout(rd_timeout* DEF_NUM_PER_SEC,
        wr_timeout* DEF_NUM_PER_SEC);
}

int Director::regSvr(int fd, ISockSvr* svr,
    long data2, const char szIP[], int port) {
    GenData* data = NULL;

    data = m_center->reg(fd);
    if (NULL != data) { 
        m_center->setData(data, svr, data2); 
        
        m_center->setAddr(data, szIP, port);

        /* just add fd for receiver */
        m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_LISTENER);
        m_center->setStat(ENUM_DIR_RECVER, data, ENUM_STAT_INVALID);

        /* disable dealer */
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_LISTENER);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);

        m_center->setTimerParam(ENUM_DIR_DEALER,
            data, &Dealer::dealerTimeoutCb, (long)m_dealer);

        /* begin receiver */
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd);

        return 0;
    } else {
        return -1;
    }
}

int Director::regCli(int fd, ISockCli* cli, 
    long data2, const char szIP[], int port) {
    GenData* data = NULL;

    data = m_center->reg(fd);
    if (NULL != data) { 
        m_center->setData(data, cli, data2);
        
        m_center->setAddr(data, szIP, port);
        m_center->setDefConnTimeout(data);

        /* just add fd for sener here */
        m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_Connector);
        m_center->setStat(ENUM_DIR_SENDER, data, ENUM_STAT_INVALID);

        /* disable dealer */
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_Connector);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);

        m_center->setTimerParam(ENUM_DIR_SENDER,
            data, &Sender::sendTimeoutCb, (long)m_sender);

        sendCommCmd(ENUM_DIR_SENDER, ENUM_CMD_ADD_FD, fd);

        return 0;
    } else {
        return -1;
    }
} 

int Director::regUdp(int fd, 
    ISockBase* base, long data2) {
    GenData* data = NULL;
    int ret = 0;
    SockName name;
    SockAddr addr;
    
    data = m_center->reg(fd);
    if (NULL != data) { 
        m_center->setData(data, base, data2);

        ret = SockTool::getLocalSock(fd, addr);
        if (0 == ret) {
            SockTool::addr2IP(&name, &addr);
            m_center->setAddr(data, name.m_ip, name.m_port); 
        }
        
        /* just add fd for receiver */
        m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_UDP);
        m_center->setStat(ENUM_DIR_RECVER, data, ENUM_STAT_INVALID);

        /* set sender thread */
        m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_UDP);
        m_center->setStat(ENUM_DIR_SENDER, data, ENUM_STAT_INVALID);

        /* disable dealer */
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_SOCK);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);

        /* begin threads */
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd); 
        sendCommCmd(ENUM_DIR_SENDER, ENUM_CMD_ADD_FD, fd);
        activate(ENUM_DIR_DEALER, data);

        return 0;
    } else {
        return -1;
    }
} 

void Director::beginSock(GenData* data, 
    unsigned rd_thresh, unsigned wr_thresh,
    bool delay) {
    int fd = m_center->getFd(data);
    
    m_center->setDefIdleTimeout(data);
    setSpeed(fd, rd_thresh, wr_thresh);
        
    /* just add fd for receiver */
    m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_SOCK);
    m_center->setStat(ENUM_DIR_RECVER, data, ENUM_STAT_INVALID);

    /* set sender thread */
    m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_SOCK);
    m_center->setStat(ENUM_DIR_SENDER, data, ENUM_STAT_INVALID);

    /* disable dealer */
    m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_SOCK);
    m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE); 

    m_center->setTimerParam(ENUM_DIR_RECVER,
        data, &Receiver::recvTimeoutCb, (long)m_receiver);
    
    m_center->setTimerParam(ENUM_DIR_SENDER,
        data, &Sender::sendTimeoutCb, (long)m_sender);

    m_center->setTimerParam(ENUM_DIR_DEALER,
        data, &Dealer::dealerTimeoutCb, (long)m_dealer);

    activate(ENUM_DIR_DEALER, data);
    sendCommCmd(ENUM_DIR_SENDER, ENUM_CMD_ADD_FD, fd);

    if (!delay) {
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd);
    } else {
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_DELAY_FD, fd);
    }
}

void Director::closeData(GenData* data) {
    int fd = m_center->getFd(data);
    bool bOk = false;
    
    bOk = m_center->unreg(data); 
    if (bOk) {
        SockTool::closeSock(fd);
    }
}

int Director::getAddr(int fd, int* pPort, 
    char ip[], int max) {
    return m_center->getAddr(fd, pPort, ip, max);
}

long Director::getExtra(int fd) {
    GenData* data = NULL;
    long extra = 0;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);

        extra = m_center->getExtra(data);
    }

    return extra;
} 

void Director::getSpeed(int fd, 
    unsigned& rd_thresh, 
    unsigned& wr_thresh) {
    GenData* data = NULL;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);

        rd_thresh = m_center->getRdThresh(data) / KILO;
        wr_thresh = m_center->getWrThresh(data) / KILO;
    } else {
        rd_thresh = 0;
        wr_thresh = 0;
    }
}

void Director::setSpeed(int fd, 
    unsigned rd_thresh, 
    unsigned wr_thresh) {
    GenData* data = NULL;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);

        m_center->setSpeed(data, rd_thresh * KILO,
            wr_thresh * KILO);
    }
} 

void Director::setMaxRdTimeout(int fd, unsigned timeout) {
    GenData* data = NULL;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);

        m_center->setMaxRdTimeout(data,
            timeout * DEF_NUM_PER_SEC);
    }
}

void Director::setMaxWrTimeout(int fd, unsigned timeout) {
    GenData* data = NULL;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);

        m_center->setMaxWrTimeout(data,
            timeout * DEF_NUM_PER_SEC);
    }
}



int Director::schedule(unsigned delay, unsigned interval,
    TimerFunc func, long data, long data2) {
    int ret = 0;
    NodeMsg* pCmd = NULL; 
    
    /* convert from sec to ticks*/
    delay *= DEF_NUM_PER_SEC;
    interval *= DEF_NUM_PER_SEC;
    pCmd = creatCmdSchedTask(delay,
        interval, func, data, data2);
    if (NULL != pCmd) {
        ret = sendCmd(ENUM_DIR_DEALER, pCmd);
    } else {
        ret = -1;
    }
    
    return ret;
}

void Director::setTimerPerSec(ITimerCb* cb) {
    m_dealer->setTimerPerSec(cb);
}

struct TaskData {
    ISockBase* m_base;
    long m_data2;
    SockName m_name;
};

static TaskData* allocTask() {
    TaskData* data = NULL;
    
    data = (TaskData*)CacheUtil::callocAlign(1, sizeof(TaskData));
    return data;
}

static void freeTask(TaskData* data) {
    CacheUtil::freeAlign(data);
}

static bool _creatCli(long arg, long arg2) {
    Director* director = (Director*)arg;
    TaskData* task = (TaskData*)arg2;
    ISockCli* cli = dynamic_cast<ISockCli*>(task->m_base);
    long data2 = task->m_data2;
    const SockName& name = task->m_name;
    
    director->creatCli(name.m_ip, name.m_port, cli, data2);

    freeTask(task);
    return false;
}

int Director::creatSvr(const char szIP[], int port, 
    ISockSvr* svr, long data2) {
    int ret = 0;
    int fd = 0; 
    SockName name;

    if (NULL == svr) {
        return -1;
    }

    SockTool::assign(name, szIP, port);
    ret = SockTool::openName(&fd, name, m_center->capacity());
    if (0 != ret) {
        return ret;
    }

    ret = regSvr(fd, svr, data2, szIP, port);
    if (0 == ret) {
        return ret;
    } else {
        SockTool::closeSock(fd);
        return -1;
    }
}

int Director::creatCli(const char szIP[], int port,
    ISockCli* cli, long data2) { 
    int ret = 0;
    int fd = 0;
    SockName name;

    if (NULL == cli) {
        return -1;
    }

    SockTool::assign(name, szIP, port);
    ret = SockTool::connName(&fd, name);
    if (0 != ret) {
        cli->onConnFail(data2, -1);
        return -1;
    }

    ret = regCli(fd, cli, data2, szIP, port);
    if (0 == ret) {
        return ret;
    } else {
        cli->onConnFail(data2, -1);
        SockTool::closeSock(fd);

        return -1;
    } 
}

int Director::sheduleCli(unsigned delay, 
    const char szIP[], int port, 
    ISockCli* base, long data2) { 
    TaskData* data = NULL;
    int ret = 0;
    
    if (NULL == base) {
        return -1;
    }

    ret = SockTool::chkAddr(szIP, port);
    if (0 != ret) {
        return -1;
    }
    
    data = allocTask();
    
    data->m_base = base;
    data->m_data2 = data2;
    SockTool::assign(data->m_name, szIP, port);

    ret = schedule(delay, 0, &_creatCli, (long)this, (long)data);
    return ret;
}

void Director::undelayRead(int fd) {
    sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_UNDELAY_FD, fd);
}

NodeMsg* Director::creatCmdComm(EnumSockCmd cmd, int fd) {
    NodeMsg* pCmd = NULL;
    CmdComm* body = NULL;
    
    pCmd = CmdUtil::creatCmd<CmdComm>(cmd);
    body = CmdUtil::getCmdBody<CmdComm>(pCmd);
    body->m_fd = fd;

    return pCmd;
}

NodeMsg* Director::creatCmdSchedTask(unsigned delay,
    unsigned interval, TimerFunc func, long data, long data2) {
    NodeMsg* pCmd = NULL;
    CmdSchedTask* body = NULL;

    pCmd = CmdUtil::creatCmd<CmdSchedTask>(ENUM_CMD_SCHED_TASK);
    body = CmdUtil::getCmdBody<CmdSchedTask>(pCmd); 
        
    body->func = func;
    body->m_data = data;
    body->m_data2 = data2;
    body->m_delay = delay;
    body->m_interval = interval;

    return pCmd;
} 

