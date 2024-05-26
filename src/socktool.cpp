#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<errno.h>
#include<cstring>
#include<cstdlib>
#include"socktool.h"
#include"shareheader.h"
#include"misc.h"


int SockTool::creatSock(int family, int type, int proto) {
    int fd = -1;

    fd = socket(family, type, proto);
    if (0 <= fd) {
        setNonblock(fd);
        setLinger(fd);
    } else {
        LOG_WARN("creat_sock| family=%d| type=%d| proto=%d|"
            " ret=%d| error=%d:%s|",
            family, type, proto,
            fd, errno, strerror(errno));
    }
    
    return fd;
}

int SockTool::creatTcp(int* pfd) {
    int ret = 0;
    int fd = -1;

    fd = creatSock(AF_INET, SOCK_STREAM, 0);
    if (0 <= fd) {
        *pfd = fd;
        ret = 0;
    } else {
        *pfd = -1;
        ret = -1;
    }

    return ret;
}

int SockTool::creatUdp(int* pfd) {
    int fd = -1;

    fd = creatSock(AF_INET, SOCK_DGRAM, 0);
    if (0 <= fd) {
        *pfd = fd;
        return 0;
    } else {
        *pfd = -1;
        return -1;
    }
}

void SockTool::setNonblock(int fd) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

bool SockTool::chkConnectStat(int fd) {
    int ret = 0;
    int errcode = 0;
	socklen_t socklen = 0;

    socklen = sizeof(errcode);
    ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &socklen);
    if (0 == ret) {
        if (0 == errcode) {
            return true;
        } else {
            LOG_INFO("chk_connect_stat| ret=%d| error=%d:%s|",
                ret, errcode, strerror(errcode));
            return false;
        }
    } else {
        LOG_INFO("chk_connect_stat| ret=%d| error=%d:%s|",
            ret, errno, strerror(errno));
        return false;
    } 
}

int SockTool::setReuse(int fd) {
    int ret = 0;
    int val = 1;
	int len = sizeof(val);
    
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, len); 
    return ret;
}

void SockTool::assign(SockName& name, const char szIP[], int port) {
    name.m_port = port;
    MiscTool::strCpy(name.m_ip, szIP, sizeof(name.m_ip));
} 

int SockTool::listenFd(int fd, int backlog) {
    int ret = 0;

    if (!(0 < backlog && MAX_BACKLOG_NUM >= backlog)) {
        backlog = MAX_BACKLOG_NUM;
    }

    ret = listen(fd, backlog);
    if (0 == ret) { 
        LOG_INFO("listen_fd| fd=%d| backlog=%d| msg=ok|",
            fd, backlog);
        return 0;
    } else {
        LOG_WARN("listen_fd| fd=%d| backlog=%d| error=%d:%s|",
            fd, backlog, errno, strerror(errno));
        return -1;
    }
}

int SockTool::bindName(int fd, const SockName& name) {
    int ret = 0;
    SockAddr addr;

    ret = ip2Addr(&addr, &name);
    if (0 != ret) {
        LOG_WARN("bind_name| ip=%s| port=%d| msg=invalid name|", 
            name.m_ip, name.m_port);
        return ret;
    }

    ret = bindAddr(fd, addr);
    if (0 != ret) { 
        LOG_WARN("bind_name| ip=%s| port=%d| msg=bind error|", 
            name.m_ip, name.m_port);
        return -1;
    }

    return ret;
}

int SockTool::bindAddr(int fd, const SockAddr& addr) {
    int ret = 0;

    ret = bind(fd, (struct sockaddr*)addr.m_addr, addr.m_addrLen);
    if (0 != ret) { 
        LOG_WARN("bind_addr| error=%d:%s|", 
            errno, strerror(errno));
        return -1;
    }

    return ret;
}

int SockTool::connName(int* pfd, const SockName& name) {
    int ret = 0;
    SockAddr addr;

    ret = ip2Addr(&addr, &name);
    if (0 != ret) {
        *pfd = -1;
        
        LOG_WARN("connect_name| ip=%s| port=%d|"
            " msg=invalid name|",
            name.m_ip, name.m_port);
        return ret;
    }

    ret = connAddr(pfd, addr);
    if (0 != ret) { 
        LOG_WARN("connect_name| ip=%s| port=%d|"
            " msg=connect error|",
            name.m_ip, name.m_port);
        return ret;
    }

    return ret;
}

int SockTool::connAddr(int* pfd, const SockAddr& addr) {
    int ret = 0;
    int fd = -1;

    do {
        ret = creatTcp(&fd);
        if (0 != ret) {
            break;
        }

        ret = connect(fd, (const struct sockaddr*)addr.m_addr,
            addr.m_addrLen);
        if (0 == ret) {
            /* ok */
        } else if (EINPROGRESS == errno) {
            /* in nonblock stat, ok */
            ret = 0;
        } else { 
            LOG_WARN("connect_addr| error=%d:%s|", 
                errno, strerror(errno));
            ret = -1;
            break;
        }

        *pfd = fd;
        return ret;
    } while (false);

    if (0 <= fd) {
        closeSock(fd);
    }
    
    return ret;
}
        
int SockTool::openAddr(int* pfd,
    const SockAddr& addr, int backlog) {
    int ret = 0;
    int fd = -1;

    do {
        ret = creatTcp(&fd);
        if (0 != ret) {
            break;
        }

        setReuse(fd);

        ret = bindAddr(fd, addr);
        if (0 != ret) {
            break;
        }

        ret = listenFd(fd, backlog);
        if (0 != ret) { 
            break;
        }

        *pfd = fd; 
        return ret;
    } while (false);

    if (0 <= fd) {
        closeSock(fd);
    }

    *pfd = -1;
    return ret;
}

int SockTool::openName(int* pfd,
    const SockName& name, int backlog) {
    int ret = 0;
    SockAddr addr;

    ret = ip2Addr(&addr, &name);
    if (0 != ret) {
        *pfd = -1;
        LOG_WARN("open_name| ip=%s| port=%d|"
            " ret=%d| msg=invalid name|",
            name.m_ip, name.m_port, ret); 
            
        return ret;
    }

    ret = openAddr(pfd, addr, backlog);
    if (0 != ret) {
        LOG_WARN("open_name| ip=%s| port=%d|"
            " ret=%d| msg=error|",
            name.m_ip, name.m_port, ret); 
            
        return ret;
    }

    LOG_INFO("open_name| ip=%s| port=%d|"
        " fd=%d| msg=ok|",
        name.m_ip, name.m_port, *pfd);
    
    return ret;
}

int SockTool::openUdpAddr(int* pfd,
    const SockAddr& addr) {
    int ret = 0;
    int fd = -1;

    do {
        ret = creatUdp(&fd);
        if (0 != ret) {
            break;
        }

        ret = bindAddr(fd, addr);
        if (0 != ret) {
            break;
        }

        *pfd = fd; 
        return ret;
    } while (false);

    if (0 <= fd) {
        closeSock(fd);
    }

    *pfd = -1;
    return ret;
}

int SockTool::openUdpName(int* pfd,
    const SockName& name) {
    int ret = 0;
    SockAddr addr;

    ret = ip2Addr(&addr, &name);
    if (0 != ret) {
        *pfd = -1;
        LOG_WARN("open_udp_name| ip=%s| port=%d|"
            " ret=%d| msg=invalid name|",
            name.m_ip, name.m_port, ret); 
            
        return ret;
    }

    ret = openUdpAddr(pfd, addr);
    if (0 != ret) {
        LOG_WARN("open_udp_name| ip=%s| port=%d|"
            " ret=%d| msg=error|",
            name.m_ip, name.m_port, ret); 
            
        return ret;
    }

    LOG_INFO("open_udp_name| ip=%s| port=%d|"
        " fd=%d| msg=ok|",
        name.m_ip, name.m_port, *pfd);
    
    return ret;
}

int SockTool::openMultiUdp(int* pfd, int port,
    const char my_ip[], const char multi_ip[]) {
    int ret = 0;
    int fd = -1;
    bool added = false;
    SockName name;

    do {
        assign(name, multi_ip, port);
        ret = openUdpName(&fd, name);
        if (0 != ret) {
            break;
        }
        
        ret = addMultiMem(fd, my_ip, multi_ip);
        if (0 != ret) {
            break;
        }

        added = true;
        
        ret = blockSrcInMulti(fd, my_ip, multi_ip, my_ip);
        if (0 != ret) {
            break;
        }

        *pfd = fd;
        return ret;
    } while (false);

    if (0 <= fd) {
        if (added) {
            dropMultiMem(fd, my_ip, multi_ip);
        }
        
        closeSock(fd);
    }

    *pfd = -1;
    return ret;
}

EnumSockRet SockTool::acceptSock(int listenFd, 
    int* pfd, SockAddr* addr) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int newFd = -1;
    socklen_t addrLen = 0;
    SockName name; 
    SockAddr tmp;
    
    MiscTool::bzero(&tmp, sizeof(SockAddr)); 
    
    addrLen = sizeof(tmp.m_addr);
    newFd = accept(listenFd, (struct sockaddr*)&tmp.m_addr, &addrLen);
    if (0 <= newFd) {
        tmp.m_addrLen = (int)addrLen;
        
        setNonblock(newFd); 
        setLinger(newFd);
        
        addr2IP(&name, &tmp); 
        LOG_INFO("accept| newfd=%d| addr_len=%d| peer=%s:%d|", 
            newFd, (int)addrLen, name.m_ip, name.m_port);
    } else if (EINTR == errno || EAGAIN == errno) {
        ret = ENUM_SOCK_RET_BLOCK;
    } else if (!(EBADF == errno || ENOTSOCK == errno
        || EINVAL == errno)){
        LOG_INFO("accept| listen_fd=%d| ret=%d|"
            " msg=%d:%s|",
            listenFd, newFd, errno, strerror(errno));
        ret = ENUM_SOCK_RET_LIMIT;
    } else {
        LOG_WARN("accept| listen_fd=%d| ret=%d|"
            " msg=%d:%s|",
            listenFd, newFd, errno, strerror(errno));
        ret = ENUM_SOCK_RET_ERR;
    } 

    if (NULL != pfd) {
        *pfd = newFd;
    }

    if (NULL != addr) {
        setAddr(*addr, tmp);
    }
    
    return ret;
}

void SockTool::closeSock(int fd) {
    if (0 < fd) {
        close(fd);
    }
}

int SockTool::chkAddr(const char szIP[], int port) {
    int ret = 0;
    SockName name;
    SockAddr addr;

    assign(name, szIP, port);
    ret = ip2Addr(&addr, &name);
    if (0 != ret) {
        LOG_ERROR("chk_addr| ip=%s:%d| err=invalid ip|",
            szIP, port);
    }

    return ret;
}

int SockTool::ip2Addr(SockAddr* addr, const SockName* name) {
    int ret = 0;
    struct sockaddr_in* sock = NULL;
    struct sockaddr_in6* sock6 = NULL;

    MiscTool::bzero(addr, sizeof(SockAddr)); 

    if (!(0 < name->m_port && name->m_port < 0x10000)) {
        LOG_ERROR("ip_to_addr| ip=%s| port=%d|"
            " err=invalid port|",
            name->m_ip, name->m_port);
        return -1;
    }
    
    sock = (struct sockaddr_in*)addr->m_addr; 
    ret = inet_pton(AF_INET, name->m_ip, &sock->sin_addr);
    if (1 == ret) {
        sock->sin_family = AF_INET;
        sock->sin_port = htons(name->m_port);
        addr->m_addrLen = sizeof(struct sockaddr_in);

        return 0;
    } else {
        MiscTool::bzero(addr->m_addr, sizeof(addr->m_addr));
        sock6 = (struct sockaddr_in6*)addr->m_addr;
    
        ret = inet_pton(AF_INET6, name->m_ip, &sock6->sin6_addr);
        if (1 == ret) {
            sock6->sin6_family = AF_INET6;
            sock6->sin6_port = htons(name->m_port);
            addr->m_addrLen = sizeof(struct sockaddr_in6);
            
            return 0;
        } else {
            LOG_ERROR("ip_to_addr| ip=%s| err=invalid ip|",
                name->m_ip);
            return -1;
        }
    }
}

int SockTool::addr2IP(SockName* name, const SockAddr* addr) {
    const char* psz = NULL;
    const struct sockaddr_in* s4 = NULL;
    const struct sockaddr_in6* s6 = NULL;
    const struct sockaddr* sock = NULL;

    MiscTool::bzero(name, sizeof(SockName));

    sock = (const struct sockaddr*)addr->m_addr;
    if (AF_INET == sock->sa_family) {
        s4 = (const struct sockaddr_in*)addr->m_addr;
        psz = inet_ntop(s4->sin_family, &s4->sin_addr, 
            name->m_ip, sizeof(name->m_ip));
        name->m_port = ntohs(s4->sin_port);
    } else {
        s6 = (const struct sockaddr_in6*)addr->m_addr;
        psz = inet_ntop(s6->sin6_family, &s6->sin6_addr, 
            name->m_ip, sizeof(name->m_ip));
        name->m_port = ntohs(s6->sin6_port);
    }
    
    if (NULL != psz && (0 < name->m_port
        && name->m_port < 0x10000)) { 
        return 0;
    } else {
        LOG_ERROR("addr_to_ip| addr_len=%d|"
            " ip=%s| port=%d| err=%d:%s|",
            addr->m_addrLen, 
            name->m_ip, name->m_port,
            errno, strerror(errno));
        return -1;
    }
}

EnumSockRet SockTool::recvTcp(int fd, void* buf,
    int max, int* prdLen) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int rdlen = 0;

    if (0 < max) {
        rdlen = recv(fd, buf, max, 0);
        if (0 < rdlen) {
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| rdlen=%d| msg=ok|",
                fd, max, rdlen); 
        } else if (0 == rdlen) {
            LOG_INFO("recvTcp| fd=%d| maxlen=%d| msg=peer closed|",
                fd, max);
            
            ret = ENUM_SOCK_RET_CLOSED;
        } else if (EAGAIN == errno || EINTR == errno) {
            LOG_VERB("recvTcp| fd=%d| maxlen=%d| msg=read empty|",
                fd, max);
            
            rdlen = 0;
            ret = ENUM_SOCK_RET_BLOCK;
        } else {
            LOG_INFO("recvTcp| fd=%d| maxlen=%d| msg=read error:%s|",
                fd, max, strerror(errno));
            
            rdlen = 0;
            ret = ENUM_SOCK_RET_ERR;
        }
    }

    if (NULL != prdLen) {
        *prdLen = rdlen;
    }

    return ret;
}

EnumSockRet SockTool::sendTcp(int fd, const void* psz,
    int max, int* pwrLen) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int sndlen = 0;

    if (0 < max) {
        sndlen = send(fd, psz, max, MSG_NOSIGNAL);
        if (0 <= sndlen) {
            LOG_DEBUG("sendTcp| fd=%d| maxlen=%d| wrlen=%d| msg=ok|",
                fd, max, sndlen);
        } else if (EAGAIN == errno || EINTR == errno) {
            LOG_VERB("sendTcp| fd=%d| maxlen=%d| msg=write block|",
                fd, max);
            
            sndlen = 0;
            ret = ENUM_SOCK_RET_BLOCK;
        } else {
            LOG_INFO("sendTcp| fd=%d| maxlen=%d| msg=write error:%s|",
                fd, max, strerror(errno));
            
            sndlen = 0;
            ret = ENUM_SOCK_RET_ERR;
        }
    }

    if (NULL != pwrLen) {
        *pwrLen = sndlen;
    }

    return ret;
}

EnumSockRet SockTool::sendTcpUntil(int fd,
    const void* buf, int size, int* pwrLen) {
    const char* psz = (const char*)buf;
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int sndlen = 0;
    int total = 0;
    
    while (0 < size && ENUM_SOCK_RET_OK == ret) { 
        ret = SockTool::sendTcp(fd, psz, size, &sndlen);
        if (0 < sndlen) { 
            psz += sndlen;
            size -= sndlen;
            total += sndlen; 
        }
    }

    if (NULL != pwrLen) {
        *pwrLen = total;
    }

    return ret;
}

EnumSockRet SockTool::sendVec(int fd, 
    struct iovec* iov, int size, 
    int* pwrLen, const SockAddr* dst) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int max = 0;
    int sndlen = 0;
    int flag = MSG_NOSIGNAL;
    struct msghdr msg;

    for (int i=0; i<size; ++i) {
        max += (int)iov[i].iov_len;
    }

    if (0 < max) {
        MiscTool::bzero(&msg, sizeof(msg));

        if (NULL != dst) {
            msg.msg_name = (void*)dst->m_addr;
            msg.msg_namelen = dst->m_addrLen;
        }
        
        msg.msg_iov = iov;
        msg.msg_iovlen = size; 

        sndlen = sendmsg(fd, &msg, flag);
        if (0 <= sndlen) {
            LOG_DEBUG("sendVec| fd=%d| size=%d|"
                " maxlen=%d| sndlen=%d| msg=ok|",
                fd, size, max, sndlen);
    	} else if (EAGAIN == errno || EINTR == errno) {
            LOG_VERB("sendVec| fd=%d| size=%d|"
                " maxlen=%d| msg=write block|",
                fd, size, max);

            sndlen = 0;
            ret = ENUM_SOCK_RET_BLOCK;
    	} else if (EMSGSIZE == errno) {
    	    /* discard a too large msg by udp */
            LOG_INFO("sendVec| fd=%d| maxlen=%d|"
                " msg=discard a too large msg|",
                fd, max);

            sndlen = max;
    	} else {
    		LOG_INFO("sendVec| fd=%d| size=%d|"
                " maxlen=%d| error=%s|",
                fd, size, max, strerror(errno));

            sndlen = 0;
            ret = ENUM_SOCK_RET_ERR;
    	}
    }

    if (NULL != pwrLen) {
        *pwrLen = sndlen;
    }

    return ret;
}

EnumSockRet SockTool::sendVecUntil(int fd, 
    struct iovec* iov, int size, 
    int* pwrLen, const SockAddr* dst) {
    char* psz = NULL;
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int total = 0;
    int wrlen = 0;
    int max = 0;

    for (int i=0; i<size; ++i) {
        max += iov[i].iov_len;
    }

    while (0 < max && ENUM_SOCK_RET_OK == ret) {
        ret = SockTool::sendVec(fd, iov, size, &wrlen, dst);
        if (0 < wrlen) {
            max -= wrlen;
            total += wrlen;
            
            while (0 < wrlen) { 
                if ((int)iov->iov_len <= wrlen) {
                    wrlen -= iov->iov_len;

                    ++iov;
                    --size;
                } else {
                    psz = (char*)iov->iov_base;
                    
                    iov->iov_base = psz + wrlen;
                    iov->iov_len -= wrlen;
                    wrlen = 0;
                }
            }
        }
    }

    if (NULL != pwrLen) {
        *pwrLen = total;
    }

    return ret;
}

EnumSockRet SockTool::recvVec(int fd,
    struct iovec* iov, int size, 
    int* prdLen, SockAddr* src) {
    EnumSockRet ret = ENUM_SOCK_RET_OK;
    int max = 0;
    int rdlen = 0;
    int addrlen = 0;
    int flag = MSG_TRUNC;
    struct msghdr msg;

    for (int i=0; i<size; ++i) {
        max += (int)iov[i].iov_len;
    }
    
    MiscTool::bzero(&msg, sizeof(msg));
    if (NULL != src) {
        MiscTool::bzero(src, sizeof(SockAddr));

        msg.msg_name = src->m_addr;
        msg.msg_namelen = sizeof(src->m_addr);
    }

    if (0 < max) { 
        msg.msg_iov = iov;
        msg.msg_iovlen = size;

        rdlen = recvmsg(fd, &msg, flag);
        if (0 < rdlen) { 
            if (!(MSG_TRUNC & msg.msg_flags)) {
                LOG_DEBUG("recvVec| fd=%d| maxlen=%d|"
                    " rdlen=%d| msg=ok|",
                    fd, max, rdlen);
            } else {
                /* discard a truncated msg by udp */
                LOG_INFO("recvVec| fd=%d| maxlen=%d|"
                    " real_len=%d| msg=discard a truncated msg|",
                    fd, max, rdlen);

                rdlen = 0;
            } 
            
            addrlen = msg.msg_namelen;
    	} else if (0 == rdlen) { 
    	    LOG_INFO("recvVec| fd=%d| maxlen=%d|"
                " msg=peer closed|",
                fd, max);
            
            addrlen = msg.msg_namelen;
    		ret = ENUM_SOCK_RET_CLOSED;
    	} else if (EAGAIN == errno || EINTR == errno) {
    	    LOG_VERB("recvVec| fd=%d| maxlen=%d| msg=read empty|",
                fd, max);
            
    		rdlen = 0; 
            ret = ENUM_SOCK_RET_BLOCK;
    	} else {
    		LOG_INFO("recvVec| fd=%d| maxlen=%d| error=%s|",
                fd, max, strerror(errno));
            
    		rdlen = 0;
            ret = ENUM_SOCK_RET_ERR;
    	}
    }

    if (NULL != prdLen) {
        *prdLen = rdlen;
    }
    
    if (NULL != src) {
        src->m_addrLen = addrlen;
    }

    return ret;
}

int SockTool::getLocalSock(int fd, SockAddr& addr) {
    int ret = 0;
    socklen_t len = 0;

    len = sizeof(addr.m_addr);
    ret = getsockname(fd, (struct sockaddr*)addr.m_addr, &len);
    if (0 == ret) { 
        addr.m_addrLen = (int)len;
    }  else {
        MiscTool::bzero(&addr, sizeof(SockAddr));
    }

    return ret;
}

int SockTool::getPeerSock(int fd, SockAddr& addr) {
    int ret = 0;
    socklen_t len = 0;

    len = sizeof(addr.m_addr);
    ret = getpeername(fd, (struct sockaddr*)addr.m_addr, &len);
    if (0 == ret) { 
        addr.m_addrLen = (int)len;
    } else {
        MiscTool::bzero(&addr, sizeof(SockAddr));
    }

    return ret;
}

int SockTool::setLinger(int fd) {
    int ret = 0;
    struct linger option;
	int len = sizeof(option);
    
    option.l_onoff = true;
    option.l_linger = 1; // 1 second 
    ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, &option, len); 
    return ret;
}

int SockTool::setSockBuf(int fd, int KB, bool isRcv) {
    int ret = 0;
    int optname = 0;
    int max = 0;
	int len = sizeof(max);
    
    if (isRcv) {
        optname = SO_RCVBUF;
    } else {
        optname = SO_SNDBUF;
    }

    /* half the input, is: (KB * 1024 / 2) */
    max = (KB << 9);
    
    ret = setsockopt(fd, SOL_SOCKET, optname, &max, len); 
    return ret;
}

int SockTool::getSockBuf(int fd, int* pKB, bool isRcv) {
    int ret = 0;
    int optname = 0;
    int max = 0;
	socklen_t len = sizeof(max);
    
    if (isRcv) {
        optname = SO_RCVBUF;
    } else {
        optname = SO_SNDBUF;
    }

    ret = getsockopt(fd, SOL_SOCKET, optname, &max, &len);
    if (0 == ret) {
        /* byte to KB unit */
        max >>= 10;
    }
    
    if (NULL != pKB) {
        *pKB = max;
    }
    
    return ret;
}

int SockTool::setMultiTtl(int fd, int ttl) {
    int ret = 0;
	int len = sizeof(ttl);

    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, len); 
    return ret;
}

int SockTool::addMultiMem(int fd, 
    const char my_ip[], const char multi_ip[]) {
    int ret = 0;
    struct ip_mreqn req;
	int len = sizeof(req);

    MiscTool::bzero(&req, sizeof(req));
    ip2Net(&req.imr_multiaddr, multi_ip);
    ip2Net(&req.imr_address, my_ip);
    
    ret = setsockopt(fd, IPPROTO_IP,
        IP_ADD_MEMBERSHIP, &req, len); 
    if (0 != ret) {
        LOG_WARN("add_multicast_mem| ret=%d|"
            " my_ip=%s| multi_ip=%s|"
            " error=%d:%s|",
            ret, my_ip, multi_ip,
            errno, strerror(errno));
    }

    setMultiTtl(fd, 1); 
    return ret;
}

int SockTool::dropMultiMem(int fd, 
    const char my_ip[], 
    const char multi_ip[]) {
    int ret = 0;
    struct ip_mreqn req;
	int len = sizeof(req);

    MiscTool::bzero(&req, sizeof(req));
    ip2Net(&req.imr_multiaddr, multi_ip);
    ip2Net(&req.imr_address, my_ip);
    
    ret = setsockopt(fd, IPPROTO_IP,
        IP_DROP_MEMBERSHIP, &req, len);
    if (0 != ret) {
        LOG_WARN("drop_multicast_mem| ret=%d|"
            " my_ip=%s| multi_ip=%s|"
            " error=%d:%s|",
            ret, my_ip, multi_ip,
            errno, strerror(errno));
    }
    
    return ret;
}

int SockTool::blockSrcInMulti(int fd, 
    const char my_ip[], 
    const char multi_ip[],
    const char src_ip[]) {
    int ret = 0;
    struct ip_mreq_source req;
	int len = sizeof(req);

    MiscTool::bzero(&req, sizeof(req));
    ip2Net(&req.imr_multiaddr, multi_ip);
    ip2Net(&req.imr_interface, my_ip);
    ip2Net(&req.imr_sourceaddr, src_ip);
    
    ret = setsockopt(fd, IPPROTO_IP,
        IP_BLOCK_SOURCE, &req, len);
    if (0 != ret) {
        LOG_WARN("block_src_in_multi| ret=%d|"
            " my_ip=%s| multi_ip=%s| src_ip=%s|"
            " error=%d:%s|",
            ret, my_ip, multi_ip, src_ip,
            errno, strerror(errno));
    }
    
    return ret;
}

void SockTool::closeMultiSock(int fd,
    const char my_ip[], const char multi_ip[]) {
    if (0 <= fd) {
        dropMultiMem(fd, my_ip, multi_ip);
        closeSock(fd);
    }
}

int SockTool::ip2Net(void* buf, const char ip[]) {
    int ret = 0;

    ret = inet_pton(AF_INET, ip, buf);
    if (1 == ret) {
        return 0;
    } else {
        return -1;
    }
}

int SockTool::net2IP(char ip[], int max, const void* net) {
    const char* psz = NULL;

    psz = inet_ntop(AF_INET, net, ip, max);
    if (NULL != psz) {
        return 0;
    } else {
        return -1;
    }
}

void SockTool::setName(SockName& dst, const SockName& src) {
    dst.m_port = src.m_port;
    MiscTool::strCpy(dst.m_ip, src.m_ip, sizeof(dst.m_ip)); 
}

void SockTool::setAddr(SockAddr& dst, const SockAddr& src) {
    dst.m_addrLen = src.m_addrLen;
    MiscTool::bcopy(dst.m_addr, src.m_addr, dst.m_addrLen);
}


