#include"sockframe.h"
#include"director.h"
#include"msgtool.h"


struct SockFrame::_intern {
    Director director;
};

SockFrame::SockFrame() {
    m_intern = NULL;
}

SockFrame::~SockFrame() {
}

static SockFrame* g_frame = NULL;

SockFrame* SockFrame::instance() { 
    int ret = 0;
    SockFrame* frame = NULL;

    if (NULL != g_frame) {
        return g_frame;
    } else {
        frame = new SockFrame();
        if (NULL != frame) {
            ret = frame->init();
            if (0 == ret) {
                g_frame = frame;

                return g_frame;
            } else {
                frame->finish();
                delete frame;
                return NULL;
            }
        } else {
            return NULL;
        }
    }
}

void SockFrame::destroy(SockFrame* frame) {
    if (NULL != frame) {
        frame->finish();
        delete frame;

        if (g_frame == frame) {
            g_frame = NULL;
        }
    }
}

int SockFrame::init() {
    int ret = 0;
    _intern* intern = NULL;
    
    intern = new _intern;
    if (NULL == intern) {
        return -1;
    }
    
    ret = intern->director.init();
    if (0 == ret) {
        m_intern = intern;
        return 0;
    } else {
        intern->director.finish();
        delete intern;
        return -1;
    }
}

void SockFrame::finish() {
    if (NULL != m_intern) {
        m_intern->director.finish();
        delete m_intern;
        m_intern = NULL;
    }
}

void SockFrame::start() {
    m_intern->director.start();
}

void SockFrame::stop() {
    m_intern->director.stop();
}

void SockFrame::wait() {
    m_intern->director.wait();
}

void SockFrame::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    m_intern->director.setTimeout(rd_timeout,
        wr_timeout);
}

long SockFrame::getExtra(int fd) {
    return m_intern->director.getExtra(fd);
}

void SockFrame::getSpeed(int fd, unsigned& rd_thresh,
        unsigned& wr_thresh) {
    m_intern->director.getSpeed(fd, rd_thresh,
        wr_thresh);
}

void SockFrame::setSpeed(int fd, unsigned rd_thresh, 
        unsigned wr_thresh) {
    m_intern->director.setSpeed(fd, rd_thresh,
        wr_thresh);
} 

int SockFrame::sendMsg(int fd, NodeMsg* pMsg) {
    return m_intern->director.sendMsg(fd, pMsg);
}

int SockFrame::dispatch(int fd, NodeMsg* pMsg) {
    return m_intern->director.dispatch(fd, pMsg);
}

void SockFrame::closeData(int fd) {
    m_intern->director.notifyClose(fd);
}

void SockFrame::undelayRead(int fd) {
    m_intern->director.undelayRead(fd);
}

int SockFrame::creatSvr(const char szIP[], int port, 
    ISockSvr* svr, long data2) {
    return m_intern->director.creatSvr(szIP, port,
        svr, data2);
}

int SockFrame::sheduleCli(unsigned delay, 
    const char szIP[], int port, 
    ISockCli* base, long data2) {
    return m_intern->director.sheduleCli(delay, 
        szIP, port, base, data2);
}

int SockFrame::creatCli(const char szIP[],
    int port, ISockCli* base, long data2) {
    return m_intern->director.creatCli(szIP,
        port, base, data2);
}

int SockFrame::schedule(unsigned delay, TFunc func,
    long data, long data2) {
    return m_intern->director.schedule(delay, func, data, data2);
}

static void sigHandler(int sig) {
    LOG_ERROR("*******sig=%d| msg=catch a signal|", sig);

    for (int i=0; i<MsgTool::maxSig(); ++i) {
        if (MsgTool::isCoreSig(sig)) {
            MsgTool::armSig(sig, NULL);
            MsgTool::raise(sig);
            return;
        }
    }

    if (NULL != g_frame) {
        g_frame->stop();
    }
}

void armSigs() {
    for (int i=1; i<MsgTool::maxSig(); ++i) {
        MsgTool::armSig(i, &sigHandler);
    }
}

