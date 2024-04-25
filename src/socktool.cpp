#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<errno.h>
#include<cstring>
#include<cstdlib>
#include"socktool.h"
#include"shareheader.h"


int SockTool::creatSock() {
    int fd = -1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 < fd) {
        fcntl(fd, F_SETFL, O_NONBLOCK);
    }
    
    return fd;
}

bool SockTool::chkConnectStat(int fd) {
    int ret = 0;
    int errcode = 0;
	socklen_t socklen = 0;

    socklen = sizeof(errcode);
    ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &socklen);
    if (0 == ret && 0 == errcode) {
        return true;
    } else {
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

int SockTool::creatListener(const char szIP[], int port, int backlog) {
    int ret = 0;
    int fd = -1;
    SockParam param;

    param.m_port = port;
    strncpy(param.m_ip, szIP, sizeof(param.m_ip) - 1);

    ret = ip2Addr(&param);
    if (0 != ret) {
        return -1;
    }
    
    fd = creatSock();
    if (0 < fd) {
        setReuse(fd);
        ret = bind(fd, (struct sockaddr*)param.m_addr, param.m_addrLen);
        if (0 != ret) {
            closeSock(fd);
            
            LOG_ERROR("bind error:%s", strerror(errno));
            return -1;
        }
        
        ret = listen(fd, backlog);
        if (0 != ret) {
            closeSock(fd);
            
            LOG_ERROR("listen error");
            return -1;
        }

        return fd;
    } else {
        return -1;
    }
}

int SockTool::creatConnector(const char szIP[], int port) {
    int ret = 0;
    int fd = -1;
    SockParam param;

    param.m_port = port;
    strncpy(param.m_ip, szIP, sizeof(param.m_ip) - 1);

    ret = ip2Addr(&param);
    if (0 != ret) {
        return -1;
    }
    
    fd = creatSock();
    if (0 < fd) {
        ret = connect(fd, (struct sockaddr*)param.m_addr, param.m_addrLen);
        if (0 == ret || EINPROGRESS == errno) {
            return fd;
        } else {
            closeSock(fd);
            
            LOG_ERROR("conn| addr=%s:%d| msg=connect error|",
                szIP, port);
            return -1;
        }
    } else { 
        return -1;
    }
}

int SockTool::acceptSock(int listenFd, 
    int* pfd, SockParam* param) {
    int newFd = -1;
    socklen_t addrLen = 0;
    struct sockaddr_in addr;

    addrLen = sizeof(addr);
    memset(&addr, 0, addrLen);
    memset(param, 0, sizeof(SockParam));
    
    newFd = accept(listenFd, (struct sockaddr*)&addr, &addrLen);
    if (0 < newFd) {
        fcntl(newFd, F_SETFL, O_NONBLOCK);
        
        param->m_addrLen = (int)addrLen;
        memcpy(param->m_addr, &addr, param->m_addrLen);
        
        addr2IP(param); 
        LOG_INFO("accept| newfd=%d| addr_len=%d| peer=%s:%d|", 
            newFd, (int)addrLen, param->m_ip, param->m_port);
        
        *pfd = newFd;
        return 1;
    } else if (EINTR == errno || EAGAIN == errno) {
        *pfd = -1;
        return 0;
    } else {
        *pfd = -1;
        return -1;
    } 
}

void SockTool::closeSock(int fd) {
    if (0 < fd) {
        close(fd);
    }
}

int SockTool::chkAddr(const char szIP[], int port) {
    int ret = 0;
    char buf[256] = {0};
    
    if (0 < port && port < 0xFFFF) {
        ret = inet_pton(AF_INET, szIP, buf);
        if (1 == ret) {
            return 0;
        } else if (0 == ret) {
            LOG_ERROR("chk_addr| ip=%s:%d| err=invalid ip|",
                szIP, port);
            return -3;
        } else {
            LOG_ERROR("chk_addr| ip=%s:%d| err=invalid format|",
                szIP, port);
            return -2;
        }
    } else {
        LOG_ERROR("chk_addr| ip=%s:%d| err=invalid port|",
            szIP, port);
        return -1;
    }
}

int SockTool::ip2Addr(SockParam* param) {
    int ret = 0;
    struct sockaddr_in* addr = (struct sockaddr_in*)param->m_addr;

    param->m_addrLen = 0;
    memset(param->m_addr, 0, sizeof(param->m_addr));

    addr->sin_family = AF_INET;
    addr->sin_port = htons(param->m_port);
    ret = inet_pton(AF_INET, param->m_ip, &addr->sin_addr);
    if (1 == ret) {
        param->m_addrLen = sizeof(struct sockaddr_in);

        return 0;
    } else if (0 == ret) {
        LOG_ERROR("ip_to_addr| ip=%s| err=invalid ip|",
            param->m_ip);
        return -1;
    } else {
        LOG_ERROR("ip_to_addr| ip=%s| err=%s|",
            param->m_ip, strerror(errno));
        return -1;
    }
}

int SockTool::addr2IP(SockParam* param) {
    const char* psz = NULL;
    struct sockaddr_in* addr = (struct sockaddr_in*)param->m_addr;

    param->m_port = ntohs(addr->sin_port);
    memset(param->m_ip, 0, sizeof(param->m_ip));
    psz = inet_ntop(AF_INET, &addr->sin_addr, param->m_ip, sizeof(param->m_ip));
    if (NULL != psz) {
        return 0;
    } else {
        LOG_ERROR("addr_to_ip| addr_len=%d| err=%s|",
            param->m_addrLen, strerror(errno));
        return -1;
    }
}

int SockTool::recvTcp(int fd, void* buf, int max) {
    int ret = 0;
    int size = 0;

    if (0 < max) {
        size = recv(fd, buf, max, 0);
        if (0 < size) {
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| rdlen=%d| msg=ok|",
                fd, max, size); 
            ret = size;
        } else if (0 == size) {
            LOG_INFO("recvTcp| fd=%d| maxlen=%d| msg=peer closed|",
                fd, max);
            
            ret = -2;
        } else if (EAGAIN == errno) {
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| msg=read empty|",
                fd, max);
            
            ret = 0;
        } else {
            LOG_INFO("recvTcp| fd=%d| maxlen=%d| msg=read error:%s|",
                fd, max, strerror(errno));
            ret = -1;
        }
    }

    return ret;
}

int SockTool::sendTcp(int fd, const void* psz, int max) {
    int ret = 0;
    int cnt = 0;

    if (0 < max) {
        cnt = send(fd, psz, max, MSG_NOSIGNAL);
        if (0 <= cnt) {
            LOG_DEBUG("sendTcp| fd=%d| maxlen=%d| wrlen=%d| msg=ok|",
                fd, max, cnt);
            ret = cnt;
        } else if (EAGAIN == errno) {
            LOG_DEBUG("sendTcp| fd=%d| maxlen=%d| msg=write block|",
                fd, max);
            ret = 0;
        } else {
            LOG_INFO("sendTcp| fd=%d| maxlen=%d| msg=write error:%s|",
                fd, max, strerror(errno));
            ret = -1;
        }
    }

    return ret;
}

