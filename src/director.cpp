#include<cstring>
#include<cstdlib>
#include"director.h"
#include"managecenter.h"
#include"receiver.h"
#include"sender.h"
#include"dealer.h"
#include"misc.h"
#include"misc.h"
#include"msgutil.h"
#include"isockapi.h"
#include"ticktimer.h"


static void readFlowctl(long arg, long arg2) {
    Receiver* receiver = (Receiver*)arg;
    GenData* data = (GenData*)arg2;
    
    receiver->flowCtlCallback(data);
}

static void writeFlowctl(long arg, long arg2) {
    Sender* sender = (Sender*)arg;
    GenData* data = (GenData*)arg2;
    
    sender->flowCtlCallback(data);
}

Director::Director() {
    m_receiver = NULL;
    m_sender = NULL;
    m_dealer = NULL;
    m_center = NULL;

    m_timer_fd = 0;
    
}

Director::~Director() {
}

int Director::init() {
    int ret = 0; 

    ret = MiscTool::creatTimer(DEF_TICK_MSEC);
    if (0 < ret) {
        m_timer_fd = ret;
    } else {
        return -1;
    } 

    m_center = new ManageCenter;
    ret = m_center->init();

    m_receiver = new Receiver(m_center, this);
    ret = m_receiver->init();
    
    m_sender = new Sender(m_center, this);
    ret = m_sender->init();
    
    m_dealer = new Dealer(m_center, this);
    ret = m_dealer->init();
  
    regTimer(m_timer_fd);

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

    if (0 < m_timer_fd) {
        SockTool::closeSock(m_timer_fd);
        m_timer_fd = 0;
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

int Director::sendCmd(EnumDir enDir, NodeCmd* pCmd) {
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

int Director::notifyTimer(EnumDir enDir, 
    GenData* data, unsigned tick) {
    int ret = 0;
    
    switch (enDir) { 
    case ENUM_DIR_SENDER:
        m_sender->notifyTimer(data, tick);
        break;

    case ENUM_DIR_DEALER:
        m_dealer->notifyTimer(data, tick);
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
    NodeCmd* pCmd = NULL; 
    
    pCmd = m_center->creatCmdComm(cmd, fd);
    ret = sendCmd(enDir, pCmd);
    return ret;
}

void Director::notifyClose(GenData* data) {
    int fd = m_center->getFd(data);
    bool action = false; 

    action = m_center->setClose(data); 
    if (action) {
        sendCommCmd(ENUM_DIR_RECVER, 
            ENUM_CMD_REMOVE_FD, fd);
    } 
}

void Director::notifyClose(int fd) {
    GenData* data = NULL;

    if (m_center->exists(fd)) {
        data = m_center->find(fd);
        notifyClose(data);
    }
}

int Director::regTimer(int fd) {
    int ret = 0;
    GenData* data = NULL;

    data = m_center->reg(fd);
    if (NULL != data) { 
        m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_TIMER);
        m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_TIMER);
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_TIMER);

        /* no need to add fd for sender and dealer */
        m_center->setStat(ENUM_DIR_SENDER, data, ENUM_STAT_IDLE);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_IDLE);

        /* just add fd for receiver */
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd);
    } else {
        ret = -1;
    }

    return ret;
}

int Director::regSvr(int fd, ISockSvr* svr, long data2,
    unsigned rd_thresh, unsigned wr_thresh,
    const char szIP[], int port) {
    int ret = 0;
    GenData* data = NULL;

    data = m_center->reg(fd);
    if (NULL != data) {
        rd_thresh *= 1024; //KB
        wr_thresh *= 1024;
        
        m_center->setData(data, (long)svr, data2);
        m_center->setSpeed(data, rd_thresh, wr_thresh);
        m_center->setAddr(data, szIP, port);

        /* just add fd for receiver */
        m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_LISTENER);

        /* disable dealer */
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_LISTENER);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);
        
        /* begin receiver */
        sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd);
    } else {
        ret = -1;
    }

    return ret;
}

void Director::setTimeout(unsigned rd_timeout, unsigned wr_timeout) {
    rd_timeout *= DEF_NUM_PER_SEC; //sec
    wr_timeout *= DEF_NUM_PER_SEC;
    
    m_center->setTimeout(rd_timeout, wr_timeout);
}

int Director::regCli(int fd, ISockCli* cli, long data2,
    unsigned rd_thresh, unsigned wr_thresh,
    const char szIP[], int port) {
    int ret = 0;
    GenData* data = NULL;

    data = m_center->reg(fd);
    if (NULL != data) {
        rd_thresh *= 1024; //KB
        wr_thresh *= 1024;
        
        m_center->setData(data, (long)cli, data2);
        m_center->setSpeed(data, rd_thresh, wr_thresh);
        m_center->setAddr(data, szIP, port);

        /* just add fd for sener here */
        m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_Connector);

        /* disable dealer */
        m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_Connector);
        m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);

        /* begin sender */
        sendCommCmd(ENUM_DIR_SENDER, ENUM_CMD_ADD_FD, fd);
    } else {
        ret = -1;
    }

    return ret;
}

void Director::setFlowctl(EnumDir enDir,
    GenData* data, TFunc func) {
        
    switch (enDir) {
    case ENUM_DIR_RECVER:
        TickTimer::setTimerCb(&data->m_rd_flowctl, 
            func, (long)m_receiver, (long)data);
        break;

    case ENUM_DIR_SENDER:
        TickTimer::setTimerCb(&data->m_wr_flowctl, 
            func, (long)m_sender, (long)data);
        break;

    case ENUM_DIR_DEALER:
    default:
        break;
    }
} 

void Director::setProto(SockProto* proto) {
    m_center->setProto(proto);
}

void Director::closeData(GenData* data) {
   int fd = m_center->getFd(data);
    
    LOG_INFO("fd=%d| addr=%s:%d| msg=closed| cnt=%u|", 
        fd, data->m_szIP, 
        data->m_port, data->m_closing);
    
    m_center->unreg(fd); 
    SockTool::closeSock(fd);
}

int Director::schedule(unsigned delay, TFunc func, 
        long data, long data2) {
    int ret = 0;
    NodeCmd* pCmd = NULL; 
    
    pCmd = m_center->creatCmdSchedTask(delay,
        func, data, data2);
    if (NULL != pCmd) {
        ret = sendCmd(ENUM_DIR_DEALER, pCmd);
    } else {
        ret = -1;
    }
    
    return ret;
}

struct TaskData {
    ISockBase* m_base;
    long m_data2;
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    int m_port;
    char m_szIP[32];
};

static void _creatCli(long arg, long arg2) {
    Director* director = (Director*)arg;
    TaskData* task = (TaskData*)arg2;
    ISockCli* cli = dynamic_cast<ISockCli*>(task->m_base);
    long data2 = task->m_data2;
    unsigned rd_thresh = task->m_rd_thresh;
    unsigned wr_thresh = task->m_wr_thresh;
    int port = task->m_port;
    const char* ip = task->m_szIP;
    
    director->creatCli(ip, port, cli, data2, rd_thresh, wr_thresh);

    free(task);
}

int Director::creatSvr(const char szIP[], int port, 
    ISockSvr* svr, long data2, 
    unsigned rd_thresh, unsigned wr_thresh) {
    int ret = 0;
    int fd = 0; 
    
    fd = SockTool::creatListener(szIP, port);
    if (0 >= fd) {
        return -1;
    }

    ret = regSvr(fd, svr, data2, rd_thresh, 
        wr_thresh, szIP, port);
    if (0 == ret) {
        return fd;
    } else {
        SockTool::closeSock(fd);
        return -1;
    }
}

int Director::creatCli(const char szIP[], int port,
    ISockCli* cli, long data2, 
    unsigned rd_thresh, unsigned wr_thresh) { 
    int ret = 0;
    int fd = 0;

    fd = SockTool::creatConnector(szIP, port);
    if (0 >= fd) {
        return -1;
    }

    ret = regCli(fd, cli, data2, rd_thresh,
        wr_thresh, szIP, port);
    if (0 == ret) {
        return fd;
    } else {
        cli->onConnFail(data2, fd);
        SockTool::closeSock(fd);

        return -1;
    } 
}

void Director::sheduleCli(unsigned delay, 
    const char szIP[], int port, 
    ISockCli* base, long data2,
    unsigned rd_thresh, unsigned wr_thresh) { 
    TaskData* data = NULL;
    
    data = new TaskData;
    data->m_base = base;
    data->m_data2 = data2;
    data->m_port = port;
    data->m_rd_thresh = rd_thresh;
    data->m_wr_thresh = wr_thresh;
    strncpy(data->m_szIP, szIP, sizeof(data->m_szIP) - 1);

    schedule(delay, &_creatCli, (long)this, (long)data);
}

void Director::onConnect(GenData* data, int result) {
    int fd = m_center->getFd(data);
    ISockCli* cli = (ISockCli*)data->m_extra; 
    long data2 = data->m_data2;
    int ret = 0;
    
    if (0 == result) {
        ret = cli->onConnOK(data2, fd);
        if (0 == ret) {
            /* reset to write */
            m_center->setCb(ENUM_DIR_SENDER, data, ENUM_WR_SOCK);
            setFlowctl(ENUM_DIR_SENDER, data, writeFlowctl);

            /* reset to read  */
            m_center->setCb(ENUM_DIR_RECVER, data, ENUM_RD_SOCK); 
            setFlowctl(ENUM_DIR_RECVER, data, readFlowctl);

            /* reset to deal */
            m_center->setCb(ENUM_DIR_DEALER, data, ENUM_DEAL_SOCK);
            m_center->setStat(ENUM_DIR_DEALER, data, ENUM_STAT_DISABLE);
            
            activate(ENUM_DIR_DEALER, data);
            activate(ENUM_DIR_SENDER, data);
            sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, fd); 
        } else {
            closeData(data);
        }
    } else {
        cli->onConnFail(data2, fd);
        closeData(data);
    } 
}

void Director::onAccept(GenData* listenData,
    int newFd, const char ip[], int port) {
    int listenFd = m_center->getFd(listenData);
    ISockSvr* svr = (ISockSvr*)listenData->m_extra;
    long data2 = listenData->m_data2;
    GenData* childData = NULL;
    int ret = 0;

    childData = m_center->reg(newFd);
    if (NULL != childData) { 
        ret = svr->onNewSock(data2, listenFd, newFd);
        if (0 == ret) {
            m_center->setData(childData, listenData->m_extra, data2);
            m_center->setSpeed(childData, 
                listenData->m_rd_thresh, 
                listenData->m_wr_thresh);
            m_center->setAddr(childData, ip, port);

            m_center->setCb(ENUM_DIR_RECVER, childData, ENUM_RD_SOCK);
            setFlowctl(ENUM_DIR_RECVER, childData, readFlowctl);
            
            /* reset to begin write */
            m_center->setCb(ENUM_DIR_SENDER, childData, ENUM_WR_SOCK);
            setFlowctl(ENUM_DIR_SENDER, childData, writeFlowctl);
            m_center->setStat(ENUM_DIR_SENDER, childData, ENUM_STAT_DISABLE);
            
            /* reset to begin deal */
            m_center->setCb(ENUM_DIR_DEALER, childData, ENUM_DEAL_SOCK);
            m_center->setStat(ENUM_DIR_DEALER, childData, ENUM_STAT_DISABLE);

            /* start all */
            activate(ENUM_DIR_DEALER, childData);
            activate(ENUM_DIR_SENDER, childData);
            sendCommCmd(ENUM_DIR_RECVER, ENUM_CMD_ADD_FD, newFd); 
        } else {
            closeData(childData);
        }
    } else {
        SockTool::closeSock(newFd);
    }
}

int Director::onProcess(GenData* data, NodeMsg* pMsg) {
    int ret = 0;
    ISockBase* base = (ISockBase*)data->m_extra;
    long data2 = data->m_data2;

    ret = base->process(data2, data->m_fd, pMsg);
    return ret;
}

void Director::onClose(GenData* data) {
    int fd = m_center->getFd(data);
    ISockBase* base = (ISockBase*)data->m_extra;
    long data2 = data->m_data2;

    base->onClose(data2, fd);
    closeData(data);
}

