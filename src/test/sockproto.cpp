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
#include"socktool.h"
#include"cache.h"
#include"misc.h"


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

    MiscTool::bcopy(psz, buf, size); 

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

        MiscTool::bcopy(psz, buffer->m_head, DEF_MSG_HEAD_SIZE); 

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
            MiscTool::bcopy(psz, input, cnt);

            used += cnt;
            buffer->m_pos = DEF_MSG_HEAD_SIZE;

            bOk = analyseHead(buffer);
            if (!bOk) { 
                return -1;
            } 
        } else {
            MiscTool::bcopy(psz, input, len);

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
        MiscTool::bcopy(psz, input, size);

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
        
        MiscTool::bcopy(psz, input, len);

        MsgTool::skipMsgPos(pMsg, len);
        used += len;
    }

    return used;
}

GenSvr::GenSvr(Config* conf) : m_conf(conf) { 
    m_frame = NULL;
    m_proto = NULL;

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
    int cap = 0;
    typeStr logDir;
    typeStr sec;

    ret = m_conf->getToken(GLOBAL_SEC, DEF_KEY_LOG_DIR, logDir);
    if (0 == ret) { 
        ret = ConfLog(logDir.c_str());
    } else {
        ret = ConfLog(NULL);
    }

    CHK_RET(ret);
    
    ret = m_conf->getNum(GLOBAL_SEC, "fd_max", cap);
    if (0 == ret) { 
        SockFrame::creat(cap);
    } else {
        SockFrame::creat();
    }
    
    m_frame = SockFrame::instance();
    m_proto = new GenSockProto(m_frame);

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
    SockBuffer* buffer = (SockBuffer*)CacheUtil::callocAlign(
        1, sizeof(SockBuffer));

    return buffer;
}

void GenSvr::freeBuffer(SockBuffer* buffer) {
    if (NULL != buffer) {
        if (NULL != buffer->m_msg) {
            MsgTool::freeMsg(buffer->m_msg);
            buffer->m_msg = NULL;
        }
        
        CacheUtil::freeAlign(buffer);
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

int GenSvr::parseData(int fd, const char* buf, int size,
    const SockAddr*) {
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
    m_first_send_cnt = 0;
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

    ret = m_conf->getNum(CLIENT_SEC, "first_send_cnt", n);
    CHK_RET(ret); 
    m_first_send_cnt = n;

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
        hd, m_first_send_cnt, m_pkg_size);
    
    opt.m_rd_thresh = m_rd_thresh;
    opt.m_wr_thresh = m_wr_thresh;
        
    for (int i=0; i<m_first_send_cnt; ++i) {
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
    const char* buf, int size,
    const SockAddr*) {
    SockBuffer* cache = (SockBuffer*)m_frame->getExtra(fd);
    int ret = 0;
    
    ret = m_proto->parseData(fd, cache, buf, size);
    return ret;
}

long GenCli::genExtra() {
    SockBuffer* cache = GenSvr::allocBuffer();

    return (long)cache;
}


GenUdp::GenUdp(Config* conf) : m_conf(conf) {
    SockFrame::creat();
    
    m_frame = SockFrame::instance();

    m_udp_fd = -1;
    
    m_rd_thresh = 0;
    m_wr_thresh = 0;
    m_rd_timeout = 0;
    m_wr_timeout = 0;

    m_pkg_size = 0;

    m_is_recv = 1;
    m_now = 0;
}

GenUdp::~GenUdp() {
    SockFrame::destroy(m_frame);
}

int GenUdp::init() {
    int ret = 0;
    int n = 0;
    typeStr logDir;
    typeStr sec;
    typeStr v;
    SockName name;

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

    ret = m_conf->getNum(UDP_SEC, "pkgSize", n);
    CHK_RET(ret); 
    m_pkg_size = n;

    ret = m_conf->getNum(UDP_SEC, "is_recv", n);
    CHK_RET(ret); 
    m_is_recv = n;

    ret = m_conf->getToken(UDP_SEC, "uni_addr", v);
    CHK_RET(ret); 

    ret = m_conf->getAddr(m_uni_info, v);
    CHK_RET(ret);

    ret = m_conf->getToken(UDP_SEC, "multi_addr", v);
    if (0 == ret) { 
        ret = m_conf->getAddr(m_multi_info, v);
        CHK_RET(ret);
    } else {
        ret = 0;
    }

    ret = m_conf->getToken(UDP_SEC, "peer_addr", v);
    if (0 == ret) { 
        ret = m_conf->getAddr(m_peer_info, v);
        CHK_RET(ret);
    } else {
        ret = 0;
    }

    if (!m_peer_info.m_ip.empty()) {
        SockTool::assign(name, m_peer_info.m_ip.c_str(),
            m_peer_info.m_port);

        ret = SockTool::ip2Addr(&m_peer_addr, &name);
        CHK_RET(ret);
    }

    m_frame->setTimerPerSec(this);
    
    return ret;
}

void GenUdp::finish() {
    if (0 <= m_udp_fd) {
        if (m_multi_info.m_ip.empty()) {
            SockTool::closeSock(m_udp_fd);
        } else {
            SockTool::closeMultiSock(m_udp_fd,
                m_uni_info.m_ip.c_str(), 
                m_multi_info.m_ip.c_str());
        }

        m_udp_fd = -1;
    }
}

void GenUdp::start() { 
    int ret = 0;
    SockName uni; 

    if (m_multi_info.m_ip.empty()) {
        SockTool::assign(uni, m_uni_info.m_ip.c_str(),
            m_uni_info.m_port);
            
        ret = SockTool::openUdpName(&m_udp_fd, uni);
    } else {
        ret = SockTool::openMultiUdp(&m_udp_fd,
            m_multi_info.m_port,
            m_uni_info.m_ip.c_str(), 
            m_multi_info.m_ip.c_str());
    } 

    if (0 == ret) {
        ret = m_frame->regUdp(m_udp_fd, this, 0);
        if (0 == ret) {
            m_frame->start();
        }
    }
    
}

void GenUdp::wait() {
    m_frame->wait();
}

void GenUdp::stop() {
    m_frame->stop();
}

void GenUdp::onClose(int) {
}

NodeMsg* GenUdp::genUdpMsg(int size,
    const SockAddr& addr) {
    static unsigned seq = 0; 
    int total = DEF_MSG_HEAD_SIZE + size;
    NodeMsg* pMsg = NULL;
    MsgHead_t* ph = NULL;

    pMsg = MsgTool::allocUdpMsg(total);
    MsgTool::setUdpAddr(pMsg, addr);
    
    ph = (MsgHead_t*)MsgTool::getMsg(pMsg);
    ph->m_size = total;
    ph->m_seq = ++seq;
    ph->m_version = DEF_MSG_VER;
    ph->m_crc = 0;
    return pMsg;
}

void GenUdp::onTimerPerSec() {
    NodeMsg* pMsg = NULL;
    
    ++m_now;
    if (!(m_now & 0x7) && !m_is_recv && !m_peer_info.m_ip.empty()) {
        LOG_INFO("on_timer_per_sec| fd=%d| pkg_size=%d|",
            m_udp_fd, m_pkg_size); 

        pMsg = genUdpMsg(m_pkg_size, m_peer_addr);
        m_frame->sendMsg(m_udp_fd, pMsg);
    } 
}

int GenUdp::process(int hd, NodeMsg* msg) {
    SockAddr* preh = NULL;
    MsgHead_t* head = NULL;
    int size = 0;
    SockName name;

    preh = MsgTool::getPreHead<SockAddr>(msg);
    head = (MsgHead_t*)MsgTool::getMsg(msg);
    size = MsgTool::getMsgSize(msg);

    SockTool::addr2IP(&name, preh);
    
    LOG_DEBUG("process_udp| fd=%d| peer=%s:%d|"
        " size=%d| seq=0x%x|",
        hd, name.m_ip, name.m_port,
        size, head->m_seq);
    return 0;
}

int GenUdp::parseData(int fd, const char* buf, int size,
    const SockAddr* addr) {
    NodeMsg* pMsg = NULL;
    SockAddr* preh = NULL;
    char* psz = NULL; 
    int ret = 0;

    if (0 < size) {
        pMsg = MsgTool::creatPreMsg<SockAddr>(size);
        preh = MsgTool::getPreHead<SockAddr>(pMsg);
        psz = MsgTool::getMsg(pMsg);

        SockTool::setAddr(*preh, *addr);
        MiscTool::bcopy(psz, buf, size); 

        MsgTool::skipMsgPos(pMsg, size);
        ret = m_frame->dispatch(fd, pMsg);
    }
    
    return ret;
}


