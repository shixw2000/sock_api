#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>
#include<cstring>
#include<cstdlib>
#include"sockproto.h"
#include"sockframe.h"
#include"isockmsg.h"
#include"msgtool.h"


GenSockProto::GenSockProto(SockFrame* frame) {
    m_frame = frame;
}

GenSockProto::~GenSockProto() {
}

int GenSockProto::dispatch(int fd, NodeMsg* pMsg) {
    int ret = 0; 

    ret = m_frame->dispatch(fd, pMsg);
    return ret;
}

int GenSockProto::parseData2(int fd,
    SockBuffer*, const char* buf, int size) {
    NodeMsg* pMsg = NULL;
    char* psz = NULL; 

    pMsg = MsgTool::allocMsg(size);
    psz = MsgTool::getMsg(pMsg);

    memcpy(psz, buf, size); 

    MsgTool::skipMsgPos(pMsg, size);
    dispatch(fd, pMsg);

    return 0; 
}

int GenSockProto::parseData(int fd,
    SockBuffer* cache, const char* buf, int size) {
    int ret = 0;
    
    while (0 < size) {
        if (cache->m_pos < DEF_MSG_HEAD_SIZE) {
            ret = parseHead(cache, buf, size);
        } else {
            ret = parseBody(fd, cache, buf, size);
        }
        
        if (0 <= ret) {
            buf += ret;
            size -= ret;
        } else {
            return ret;
        }
    }

    return 0;
}

bool GenSockProto::analyseHead(SockBuffer* buffer) {
    int size = 0;
    NodeMsg* pMsg = NULL;
    MsgHead_t* ph = (MsgHead_t*)buffer->m_head;
    char* psz = NULL;
    
    size = ph->m_size;
    if (DEF_MSG_HEAD_SIZE < size && size < MAX_MSG_SIZE) {
        pMsg = MsgTool::allocMsg(size);
        psz = MsgTool::getMsg(pMsg);

        memcpy(psz, buffer->m_head, DEF_MSG_HEAD_SIZE); 

        MsgTool::skipMsgPos(pMsg, DEF_MSG_HEAD_SIZE);
        buffer->m_msg = pMsg;
        return true;
    } else {
        return false;
    }
}

int GenSockProto::parseHead(SockBuffer* buffer,
    const char* input, int len) {
    int cnt = 0;
    int used = 0;
    bool bOk = false;
    char* psz = NULL;
    
    if (buffer->m_pos < DEF_MSG_HEAD_SIZE) {
        cnt = DEF_MSG_HEAD_SIZE - buffer->m_pos;
        psz = &buffer->m_head[buffer->m_pos];
        
        if (cnt <= len) {
            memcpy(psz, input, cnt);

            used += cnt;
            buffer->m_pos = DEF_MSG_HEAD_SIZE;

            bOk = analyseHead(buffer);
            if (!bOk) { 
                return -1;
            } 
        } else {
            memcpy(psz, input, len);

            used += len;
            buffer->m_pos += len;
        }
    } 

    return used;
}

int GenSockProto::parseBody(int fd, 
    SockBuffer* buffer, const char* input, int len) {
    int size = 0;
    int pos = 0;
    int used = 0;
    NodeMsg* pMsg = NULL;
    char* psz = NULL;

    pMsg = buffer->m_msg;
    
    size = MsgTool::getMsgSize(pMsg);
    pos = MsgTool::getMsgPos(pMsg);
    psz = MsgTool::getMsg(pMsg);
    
    psz += pos;
    size -= pos;
    if (size <= len) {
        /* get a completed msg */
        memcpy(psz, input, size);

        MsgTool::skipMsgPos(pMsg, size);
        used += size; 

        LOG_DEBUG("parse_msg| fd=%d| len=%d| size=%d| pos=%d|",
            fd, len, size, pos);

        /* dispatch a completed msg */
        dispatch(fd, pMsg);
        buffer->m_msg = NULL;
        buffer->m_pos = 0;
    } else {
        LOG_DEBUG("skip_msg| fd=%d| len=%d| size=%d| pos=%d|",
            fd, len, size, pos);
        
        memcpy(psz, input, len);

        MsgTool::skipMsgPos(pMsg, len);
        used += len;
    }

    return used;
}

GenSvr::GenSvr(unsigned rd_thresh, 
    unsigned wr_thresh) {
    m_frame = SockFrame::instance();
    m_accpt = new GenAccpt(m_frame);

    m_rd_thresh = rd_thresh;
    m_wr_thresh = wr_thresh;
}

GenSvr::~GenSvr() {
    if (NULL != m_accpt) {
        delete m_accpt;
    }
}

SockBuffer* GenSvr::allocBuffer() {
    SockBuffer* buffer = (SockBuffer*)calloc(1, sizeof(SockBuffer));

    return buffer;
}

void GenSvr::freeBuffer(SockBuffer* buffer) {
    if (NULL != buffer) {
        if (NULL != buffer->m_msg) {
            MsgTool::freeMsg(buffer->m_msg);
            buffer->m_msg = NULL;
        }
        
        free(buffer);
    }
}

int GenSvr::onNewSock(int, int, 
    AccptOption& opt) {
    SockBuffer* buffer = allocBuffer();

    opt.m_sock = m_accpt;
    opt.m_extra = (long)buffer;
    opt.m_rd_thresh = m_rd_thresh;
    opt.m_wr_thresh = m_wr_thresh;

    return 0;
}

void GenSvr::onClose(int) { 
}

GenSvr::GenAccpt::GenAccpt(SockFrame* frame) {
    m_frame = frame;
    m_proto = new GenSockProto(frame);
}

GenSvr::GenAccpt::~GenAccpt() {
    if (NULL != m_proto) {
        delete m_proto;
    }
}

void GenSvr::GenAccpt::onClose(int hd) {
    SockBuffer* buffer = (SockBuffer*)m_frame->getExtra(hd);

    GenSvr::freeBuffer(buffer);
}

int GenSvr::GenAccpt::process(int hd, NodeMsg* msg) {
    MsgHead_t* head = NULL;

    head = (MsgHead_t*)MsgTool::getMsg(msg);

    (void)hd;
    (void)head;
    LOG_DEBUG("process_msg| fd=%d| seq=0x%x|",
        hd, head->m_seq++);

    NodeMsg* ref = MsgTool::refNodeMsg(msg);
    m_frame->sendMsg(hd, ref);
    return 0;
}

int GenSvr::GenAccpt::parseData(int fd, 
    const char* buf, int size) {
    SockBuffer* cache = (SockBuffer*)m_frame->getExtra(fd);
    int ret = 0;
    
    ret = m_proto->parseData(fd, cache, buf, size);
    return ret;
}


GenCli::GenCli(unsigned rd_thresh, 
        unsigned wr_thresh,
        int pkgSize, int pkgCnt) { 
    m_frame = SockFrame::instance();
    m_proto = new GenSockProto(m_frame);
    
    m_rd_thresh = rd_thresh;
    m_wr_thresh = wr_thresh;
    m_pkg_size = pkgSize;
    m_pkg_cnt = pkgCnt;
}

GenCli::~GenCli() {
    if (NULL != m_proto) {
        delete m_proto;
    }
}

NodeMsg* GenCli::genMsg(int size) {
    static unsigned seq = 0; 
    int total = DEF_MSG_HEAD_SIZE + size;
    NodeMsg* pMsg = NULL;
    MsgHead_t* ph = NULL;

    pMsg = MsgTool::allocMsg(total);
    ph = (MsgHead_t*)MsgTool::getMsg(pMsg);
    ph->m_size = total;
    ph->m_seq = ++seq;
    ph->m_version = DEF_MSG_VER;
    ph->m_crc = 0;
    return pMsg;
}

void GenCli::onClose(int hd) {
    SockBuffer* cache = (SockBuffer*)m_frame->getExtra(hd);
    
    GenSvr::freeBuffer(cache);
}

void GenCli::onConnFail(long extra, int) {
    SockBuffer* cache = (SockBuffer*)extra;
    
    GenSvr::freeBuffer(cache);
}

int GenCli::onConnOK(int hd,
    ConnOption& opt) {
    NodeMsg* pMsg = NULL;

    LOG_INFO("conn_ok| fd=%d| pkg_cnt=%d| pkg_size=%d|",
        hd, m_pkg_cnt, m_pkg_size);
    
    opt.m_rd_thresh = m_rd_thresh;
    opt.m_wr_thresh = m_wr_thresh;
        
    for (int i=0; i<m_pkg_cnt; ++i) {
        pMsg = genMsg(m_pkg_size);
        m_frame->sendMsg(hd, pMsg);
    }

    return 0;
}

int GenCli::process(int hd, NodeMsg* msg) {
    MsgHead_t* head = NULL;

    head = (MsgHead_t*)MsgTool::getMsg(msg);

    (void)hd;
    (void)head;
    LOG_DEBUG("cli_msg| fd=%d| seq=0x%x|",
        hd, head->m_seq++);

    NodeMsg* ref = MsgTool::refNodeMsg(msg);
    m_frame->sendMsg(hd, ref);
    return 0;
}

int GenCli::parseData(int fd, 
    const char* buf, int size) {
    SockBuffer* cache = (SockBuffer*)m_frame->getExtra(fd);
    int ret = 0;
    
    ret = m_proto->parseData(fd, cache, buf, size);
    return ret;
}

long GenCli::genExtra() {
    SockBuffer* cache = GenSvr::allocBuffer();

    return (long)cache;
}


