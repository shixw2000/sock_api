#include"sockframe.h"
#include"director.h"
#include"misc.h"
#include"socktool.h"


struct SockFrame::_intern {
    Director director;
};

SockFrame::SockFrame() {
    m_bValid = false;
    m_intern = NULL;
}

SockFrame::~SockFrame() {
}

static SockFrame* g_frame = NULL;

SockFrame* SockFrame::instance() { 
    return g_frame;
}

void SockFrame::creat(int cap) {
    if (NULL == g_frame) {
        g_frame = new SockFrame();

        g_frame->init(cap);
    }
}

void SockFrame::destroy(SockFrame* frame) {
    if (NULL != frame && frame == g_frame) {
        g_frame = NULL;
        
        frame->finish();
        delete frame; 
    }
}

int SockFrame::init(int cap) {
    int ret = 0;
    _intern* intern = NULL;
    
    intern = new _intern;
    if (NULL == intern) {
        return -1;
    }
    
    ret = intern->director.init(cap);
    if (0 == ret) {
        m_intern = intern;
        m_bValid = true;
        return 0;
    } else {
        intern->director.finish();
        delete intern;
        return -1;
    }
}

void SockFrame::finish() {
    if (NULL != m_intern) {
        m_bValid = false;
        
        m_intern->director.finish();
        delete m_intern;
        m_intern = NULL;
    }
}

bool SockFrame::chkValid() const {
    if (m_bValid) {
        return true;
    } else {
        return false;
    }
}

void SockFrame::start() {
    if (chkValid()) {
        m_intern->director.start();
    } else {
        LOG_ERROR("start_frame| msg=invalid frame|");
    }
}

void SockFrame::stop() {
    if (chkValid()) {
        m_intern->director.stop();
    } else {
        LOG_ERROR("stop_frame| msg=invalid frame|");
    }
}

void SockFrame::wait() {
    if (chkValid()) {
        m_intern->director.wait();
    } else {
        LOG_ERROR("wait_frame| msg=invalid frame|");
    }
}

void SockFrame::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    if (chkValid()) {
        m_intern->director.setTimeout(rd_timeout,
            wr_timeout);
    } 
}

void SockFrame::setMaxRdTimeout(int fd, unsigned timeout) {
    if (chkValid()) {
        m_intern->director.setMaxRdTimeout(fd, timeout);
    }
}

void SockFrame::setMaxWrTimeout(int fd, unsigned timeout) {
    if (chkValid()) {
        m_intern->director.setMaxWrTimeout(fd, timeout);
    }
}

long SockFrame::getExtra(int fd) {
    if (chkValid()) {
        return m_intern->director.getExtra(fd);
    } else {
        return 0;
    }
}

int SockFrame::getAddr(int fd, int* pPort, 
    char ip[], int max) {
    return m_intern->director.getAddr(fd, pPort, ip, max);
}

void SockFrame::getSpeed(int fd, unsigned& rd_thresh,
    unsigned& wr_thresh) {
    if (chkValid()) {
        m_intern->director.getSpeed(fd, rd_thresh,
            wr_thresh);
    }
}

void SockFrame::setSpeed(int fd, unsigned rd_thresh, 
    unsigned wr_thresh) {
    if (chkValid()) {
        m_intern->director.setSpeed(fd, rd_thresh,
            wr_thresh);
    }
} 

int SockFrame::sendMsg(int fd, NodeMsg* pMsg) {
    if (chkValid()) {
        return m_intern->director.sendMsg(fd, pMsg);
    } else {
        return -1;
    }
}

int SockFrame::dispatch(int fd, NodeMsg* pMsg) {
    if (chkValid()) {
        return m_intern->director.dispatch(fd, pMsg);
    } else {
        return -1;
    }
}

void SockFrame::closeData(int fd) {
    if (chkValid()) {
        m_intern->director.closeData(fd);
    }
}

void SockFrame::undelayRead(int fd) {
    if (chkValid()) {
        m_intern->director.undelayRead(fd);
    }
}

int SockFrame::creatSvr(const char szIP[], int port, 
    ISockSvr* svr, long data2) {
    if (chkValid()) {
        return m_intern->director.creatSvr(szIP, port,
            svr, data2);
    } else {
        return -1;
    }
}

int SockFrame::regUdp(int fd, 
    ISockBase* base, long data2) {
    if (chkValid()) {
        return m_intern->director.regUdp(fd, 
            base, data2);
    } else {
        return -1;
    }
}

int SockFrame::sheduleCli(unsigned delay, 
    const char szIP[], int port, 
    ISockCli* base, long data2) {
    if (chkValid()) {
        return m_intern->director.sheduleCli(delay, 
            szIP, port, base, data2);
    } else {
        return -1;
    }
}

int SockFrame::creatCli(const char szIP[],
    int port, ISockCli* base, long data2) {
    if (chkValid()) {
        return m_intern->director.creatCli(szIP,
            port, base, data2);
    } else {
        return -1;
    }
}

unsigned SockFrame::now() const {
    if (chkValid()) {
        return m_intern->director.now();
    } else {
        return 0;
    }
}

void SockFrame::startTimer(TimerObj* ele, 
    unsigned delay_sec, unsigned interval_sec) {
    if (chkValid()) {
        m_intern->director.startTimer(
            ele, delay_sec, interval_sec);
    }
}

void SockFrame::restartTimer(TimerObj* ele, 
    unsigned delay_sec, unsigned interval_sec) {
    if (chkValid()) {
        m_intern->director.restartTimer(
            ele, delay_sec, interval_sec);
    }
}

void SockFrame::stopTimer(TimerObj* ele) {
    if (chkValid()) {
        m_intern->director.stopTimer(ele);
    }
}

void SockFrame::setParam(TimerObj* ele, TimerFunc cb, 
    long data, long data2) {
    if (chkValid()) {
        m_intern->director.setParam(ele, cb, data, data2);
    }
}

TimerObj* SockFrame::allocTimer() {
    if (chkValid()) {
        return m_intern->director.allocTimer();
    } else {
        return NULL;
    }
}
void SockFrame::freeTimer(TimerObj* ele) {
    if (chkValid()) {
        m_intern->director.freeTimer(ele);
    }
}

static void sigHandler(int sig) {
    LOG_ERROR("*******sig=%d| msg=catch a signal|", sig);

    for (int i=0; i<MiscTool::maxSig(); ++i) {
        if (MiscTool::isCoreSig(sig)) {
            MiscTool::armSig(sig, NULL);
            MiscTool::raise(sig);
            return;
        }
    }

    if (NULL != SockFrame::instance()) {
        SockFrame::instance()->stop();
    }
}

void armSigs() {
    for (int i=1; i<MiscTool::maxSig(); ++i) {
        MiscTool::armSig(i, &sigHandler);
    }
}

