#include"sockframe.h"
#include"director.h"
#include"msgutil.h"


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

NodeMsg* SockFrame::creatNodeMsg(int size) {
    return MsgUtil::creatNodeMsg(size);
}

void SockFrame::freeNodeMsg(NodeMsg* pb) {
    MsgUtil::freeNodeMsg(pb);
}

bool SockFrame::completedMsg(NodeMsg* pb) {
    return MsgUtil::completedMsg(pb);
}

char* SockFrame::getMsg(const NodeMsg* pb) {
    return MsgUtil::getMsg(pb);
}

NodeMsg* SockFrame::refNodeMsg(NodeMsg* pb) {
    return MsgUtil::refNodeMsg(pb);
}

int SockFrame::getMsgSize(const NodeMsg* pb) {
    return MsgUtil::getMsgSize(pb);
}

int SockFrame::getMsgPos(const NodeMsg* pb) {
    return MsgUtil::getMsgPos(pb);
}

void SockFrame::setMsgPos(NodeMsg* pb, int pos) {
    MsgUtil::setMsgPos(pb, pos);
}

void SockFrame::skipMsgPos(NodeMsg* pb, int pos) {
    MsgUtil::skipMsgPos(pb, pos);
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

void SockFrame::setProto(SockProto* proto) {
    m_intern->director.setProto(proto);
}

void SockFrame::setTimeout(unsigned rd_timeout,
    unsigned wr_timeout) {
    m_intern->director.setTimeout(rd_timeout,
        wr_timeout);
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

int SockFrame::creatSvr(const char szIP[], int port, 
    ISockSvr* svr, long data2, 
    unsigned rd_thresh, unsigned wr_thresh) {
    return m_intern->director.creatSvr(szIP, port,
        svr, data2, rd_thresh, wr_thresh);
}

void SockFrame::sheduleCli(unsigned delay, const char szIP[],
    int port, ISockCli* base, long data2,
    unsigned rd_thresh, unsigned wr_thresh) {
    m_intern->director.sheduleCli(delay, szIP, port,
        base, data2, rd_thresh, wr_thresh);
}

int SockFrame::creatCli(const char szIP[], int port,
    ISockCli* base, long data2, 
    unsigned rd_thresh, unsigned wr_thresh) {
    return m_intern->director.creatCli(szIP, port,
        base, data2, rd_thresh, wr_thresh);
}

int SockFrame::schedule(unsigned delay, TFunc func,
    long data, long data2) {
    return m_intern->director.schedule(delay, func, data, data2);
}

