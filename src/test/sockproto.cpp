#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>
#include<cstring>
#include<cstdlib>
#include"sockproto.h"
#include"sockframe.h"
#include"isockmsg.h"


GenSockProto::GenSockProto() {
    m_frame = SockFrame::instance();
}

GenSockProto::~GenSockProto() {
}

int GenSockProto::dispatch(int fd, NodeMsg* pMsg) {
    int ret = 0; 

    ret = m_frame->dispatch(fd, pMsg);
    return ret;
}

bool GenSockProto::parseData2(int fd,
    SockBuffer*, const char* buf, int size) {
    NodeMsg* pMsg = NULL;
    char* psz = NULL; 

    pMsg = SockFrame::creatNodeMsg(size);
    psz = SockFrame::getMsg(pMsg);

    memcpy(psz, buf, size); 

    SockFrame::skipMsgPos(pMsg, size);
    dispatch(fd, pMsg);

    return size; 
}

bool GenSockProto::parseData(int fd,
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
            return false;
        }
    }

    return true;
}

bool GenSockProto::analyseHead(SockBuffer* buffer) {
    int size = 0;
    NodeMsg* pMsg = NULL;
    MsgHead_t* ph = (MsgHead_t*)buffer->m_head;
    char* psz = NULL;
    
    size = ph->m_size;
    if (DEF_MSG_HEAD_SIZE < size) {
        pMsg = SockFrame::creatNodeMsg(size);
        psz = SockFrame::getMsg(pMsg);

        memcpy(psz, buffer->m_head, DEF_MSG_HEAD_SIZE); 

        SockFrame::skipMsgPos(pMsg, DEF_MSG_HEAD_SIZE);
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
    
    size = SockFrame::getMsgSize(pMsg);
    pos = SockFrame::getMsgPos(pMsg);
    psz = SockFrame::getMsg(pMsg);
    
    psz += pos;
    size -= pos;
    if (size <= len) {
        /* get a completed msg */
        memcpy(psz, input, size);

        SockFrame::skipMsgPos(pMsg, size);
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

        SockFrame::skipMsgPos(pMsg, len);
        used += len;
    }

    return used;
}

GenSvr::GenSvr() {
    m_frame = SockFrame::instance();
}

int GenSvr::onNewSock(long, int, int) {
    return 0;
}

void GenSvr::onClose(long, int) {
}

int GenSvr::process(long, int hd, NodeMsg* msg) {
    MsgHead_t* head = NULL;

    head = (MsgHead_t*)SockFrame::getMsg(msg);

    (void)hd;
    (void)head;
    LOG_DEBUG("process_msg| fd=%d| seq=0x%x|",
        hd, head->m_seq++);

    NodeMsg* ref = SockFrame::refNodeMsg(msg);
    m_frame->sendMsg(hd, ref);
    return 0;
}


GenCli::GenCli(int pkgSize, int pkgCnt) {
    m_pkg_size = pkgSize;
    m_pkg_cnt = pkgCnt;
    m_frame = SockFrame::instance();
}

NodeMsg* GenCli::genMsg(int size) {
    static unsigned seq = 0; 
    int total = DEF_MSG_HEAD_SIZE + size;
    NodeMsg* pMsg = NULL;
    MsgHead_t* ph = NULL;

    pMsg = SockFrame::creatNodeMsg(total);
    ph = (MsgHead_t*)SockFrame::getMsg(pMsg);
    ph->m_size = total;
    ph->m_seq = ++seq;
    ph->m_version = DEF_MSG_VER;
    ph->m_crc = 0;
    return pMsg;
}

void GenCli::onClose(long, int) {
}

void GenCli::onConnFail(long, int) {
}

int GenCli::onConnOK(long, int hd) {
    NodeMsg* pMsg = NULL;

    (void)hd;
    LOG_INFO("conn_ok| fd=%d| pkg_cnt=%d| pkg_size=%d|",
        hd, m_pkg_cnt, m_pkg_size);
    
    for (int i=0; i<m_pkg_cnt; ++i) {
        pMsg = genMsg(m_pkg_size);
        m_frame->sendMsg(hd, pMsg);
    }
    
    return 0;
}

int GenCli::process(long, int hd, NodeMsg* msg) {
    MsgHead_t* head = NULL;

    head = (MsgHead_t*)SockFrame::getMsg(msg);

    (void)hd;
    (void)head;
    LOG_DEBUG("cli_msg| fd=%d| seq=0x%x|",
        hd, head->m_seq++);

    NodeMsg* ref = SockFrame::refNodeMsg(msg);
    m_frame->sendMsg(hd, ref);
    return 0;
}


