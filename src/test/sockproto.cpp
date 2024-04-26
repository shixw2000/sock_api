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
        if ((int)cache->m_pos < DEF_MSG_HEAD_SIZE) {
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
    
    if ((int)buffer->m_pos < DEF_MSG_HEAD_SIZE) {
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

GenSvr::GenSvr(Config* conf) : m_conf(conf) {
    SockFrame::creat();
    
    m_frame = SockFrame::instance();
    m_proto = new GenSockProto(m_frame);

    m_rd_thresh = 0;
    m_wr_thresh = 0;
    m_rd_timeout = 0;
    m_wr_timeout = 0;
}

GenSvr::~GenSvr() {
    if (NULL != m_proto) {
        delete m_proto;
    }

    SockFrame::destroy(m_frame);
}

int GenSvr::init() {
    int ret = 0;
    int n = 0;
    typeStr logDir;
    typeStr sec;

    ret = m_conf->getToken(GLOBAL_SEC, DEF_KEY_LOG_DIR, logDir);
    if (0 == ret) { 
        ret = ConfLog(logDir.c_str());
    } else {
        ret = ConfLog(NULL);
    }
    
    CHK_RET(ret);

    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_LEVEL_NAME, n);
    CHK_RET(ret); 
    m_log_level = n;

    SetLogLevel(m_log_level);

    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_STDIN_NAME, n);
    CHK_RET(ret); 
    m_log_stdin = n;

    SetLogScreen(m_log_stdin);

    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_FILE_SIZE, n);
    CHK_RET(ret); 
    m_log_size = n;

    SetMaxLogSize(m_log_size);

    ret = m_conf->getNum(GLOBAL_SEC, "rd_thresh", n);
    CHK_RET(ret); 
    m_rd_thresh = n;

    ret = m_conf->getNum(GLOBAL_SEC, "wr_thresh", n);
    CHK_RET(ret); 
    m_wr_thresh = n;

    ret = m_conf->getNum(GLOBAL_SEC, "rd_timeout", n);
    CHK_RET(ret); 
    m_rd_timeout = n;

    ret = m_conf->getNum(GLOBAL_SEC, "wr_timeout", n);
    CHK_RET(ret); 
    m_wr_timeout = n;

    ret = m_conf->getToken(SERVER_SEC, "listen_addr", sec);
    CHK_RET(ret); 

    ret = m_conf->getAddr(m_addr, sec);
    CHK_RET(ret);
    
    return ret;
}

void GenSvr::finish() {
}

void GenSvr::start() { 
    m_frame->setTimeout(m_rd_timeout, m_wr_timeout);

    m_frame->creatSvr(m_addr.m_ip.c_str(), m_addr.m_port, this, 0);
    
    m_frame->start();
}

void GenSvr::wait() {
    m_frame->wait();
}

void GenSvr::stop() {
    m_frame->stop();
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

    opt.m_extra = (long)buffer;
    opt.m_rd_thresh = m_rd_thresh;
    opt.m_wr_thresh = m_wr_thresh;

    return 0;
}

void GenSvr::onListenerClose(int) { 
}

void GenSvr::onClose(int hd) {
    SockBuffer* buffer = (SockBuffer*)m_frame->getExtra(hd);

    GenSvr::freeBuffer(buffer);
}

int GenSvr::process(int hd, NodeMsg* msg) {
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

int GenSvr::parseData(int fd, const char* buf, int size) {
    SockBuffer* cache = (SockBuffer*)m_frame->getExtra(fd);
    int ret = 0;
    
    ret = m_proto->parseData(fd, cache, buf, size);
    return ret;
}


GenCli::GenCli(Config* conf) : m_conf(conf) {
    SockFrame::creat();
    
    m_frame = SockFrame::instance();
    m_proto = new GenSockProto(m_frame);
    
    m_rd_thresh = 0;
    m_wr_thresh = 0;
    m_rd_timeout = 0;
    m_wr_timeout = 0;
    m_pkg_size = 0;
    m_pkg_cnt = 0;
    m_cli_cnt = 1;
}

GenCli::~GenCli() {
    if (NULL != m_proto) {
        delete m_proto;
    }

    SockFrame::destroy(m_frame);
}

int GenCli::init() {
    int ret = 0;
    int n = 0;
    typeStr logDir;
    typeStr sec;

    ret = m_conf->getToken(GLOBAL_SEC, "log_dir", logDir);
    if (0 == ret) { 
        ret = ConfLog(logDir.c_str());
    } else {
        ret = ConfLog(NULL);
    }
    
    CHK_RET(ret);
    
    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_LEVEL_NAME, n);
    CHK_RET(ret); 
    m_log_level = n;

    SetLogLevel(m_log_level);

    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_STDIN_NAME, n);
    CHK_RET(ret); 
    m_log_stdin = n;

    SetLogScreen(m_log_stdin);

    ret = m_conf->getNum(GLOBAL_SEC, DEF_KEY_LOG_FILE_SIZE, n);
    CHK_RET(ret); 
    m_log_size = n;

    SetMaxLogSize(m_log_size);

    ret = m_conf->getNum(GLOBAL_SEC, "rd_thresh", n);
    CHK_RET(ret); 
    m_rd_thresh = n;

    ret = m_conf->getNum(GLOBAL_SEC, "wr_thresh", n);
    CHK_RET(ret); 
    m_wr_thresh = n;

    ret = m_conf->getNum(GLOBAL_SEC, "rd_timeout", n);
    CHK_RET(ret); 
    m_rd_timeout = n;

    ret = m_conf->getNum(GLOBAL_SEC, "wr_timeout", n);
    CHK_RET(ret); 
    m_wr_timeout = n;

    ret = m_conf->getNum(CLIENT_SEC, "pkgSize", n);
    CHK_RET(ret); 
    m_pkg_size = n;

    ret = m_conf->getNum(CLIENT_SEC, "pkgCnt", n);
    CHK_RET(ret); 
    m_pkg_cnt = n;

    ret = m_conf->getNum(CLIENT_SEC, "cliCnt", n);
    CHK_RET(ret); 
    m_cli_cnt = n;

    ret = m_conf->getToken(CLIENT_SEC, "conn_addr", sec);
    CHK_RET(ret); 

    ret = m_conf->getAddr(m_addr, sec);
    CHK_RET(ret);
    
    return ret;
}

void GenCli::finish() {
}

void GenCli::start() {
    m_frame->setTimeout(m_rd_timeout, m_wr_timeout);

    for (int i=0; i<m_cli_cnt; ++i) {
        m_frame->sheduleCli(1, m_addr.m_ip.c_str(), 
            m_addr.m_port, this, genExtra());
    }
    
    m_frame->start(); 
}

void GenCli::wait() {
    m_frame->wait();
}

void GenCli::stop() {
    m_frame->stop();
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


