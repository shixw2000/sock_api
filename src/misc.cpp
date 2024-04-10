#include<unistd.h>
#include<cstdio>
#include<cstring>
#include<sys/time.h>
#include<time.h>
#include<stdarg.h>
#include<cstdlib>
#include<signal.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/eventfd.h>
#include<sys/timerfd.h>
#include<arpa/inet.h> 
#include<unistd.h>
#include<errno.h>
#include<fcntl.h> 
#include"misc.h"
#include"cthread.h"


#define __INIT_RUN__ __attribute__((unused, constructor))

static ClockTime g_origin_tm = INIT_CLOCK_TIME;

static void _getTime(ClockTime* ct) {
    int ret = 0;
    struct timeval tv;

    ret = gettimeofday(&tv, NULL);
    if (0 == ret) {
        ct->m_sec = tv.tv_sec;
        ct->m_msec = tv.tv_usec/1000;
    } else {
        ct->m_sec = 0;
        ct->m_msec = 0;
    }
}

static __INIT_RUN__ void initTool() {
    ClockTime ct1;

    MiscTool::getClock(&ct1);
    _getTime(&g_origin_tm);

    if (g_origin_tm.m_msec >= ct1.m_msec) {
        g_origin_tm.m_sec -= ct1.m_sec;
        g_origin_tm.m_msec -= ct1.m_msec;
    } else {
        g_origin_tm.m_sec -= ct1.m_sec + 1;
        g_origin_tm.m_msec += 1000 - ct1.m_msec;
    } 
} 

static int timeStamp(char stamp[], int max) {
    int cnt = 0;
    ClockTime ct1;
    time_t in;
    struct tm out;

    MiscTool::getTime(&ct1);

    in = ct1.m_sec;
    memset(&out, 0, sizeof(out));
    localtime_r(&in, &out);
    cnt = snprintf(stamp, max, "<%d-%02d-%02d %02d:%02d:%02d.%u %d> ",
        out.tm_year + 1900, out.tm_mon + 1,
        out.tm_mday, out.tm_hour,
        out.tm_min, out.tm_sec,
        ct1.m_msec,
        CThread::getTid());
    if (0 < max && 0 < cnt) {
        if (cnt < max) {
            return cnt;
        } else {
            stamp[--max] = '\0';
            return max;
        }
    } else {
        stamp[0] = '\0';
        return 0;
    }
}

void log_screen(const char format[], ...) {
    int max = 0;
    int cnt = 0;
    char buf[1024];
    char* psz = NULL;
    va_list ap;

    max = (int)sizeof(buf) - 1;
    buf[max] = '\0';
    psz = buf;
    
    cnt = timeStamp(psz, max);
    psz += cnt;
    max -= cnt;
    
    va_start(ap, format);
    cnt = vsnprintf(psz, max, format, ap);
    va_end(ap);

    fprintf(stderr, "%s\n", buf);
}

void MiscTool::getTime(ClockTime* ct) {
    getClock(ct);

    clock2Time(ct, ct);
}

void MiscTool::getClock(ClockTime* ct) {
    int ret = 0;
    struct timespec tp;

    ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (0 == ret) {
        ct->m_sec = tp.tv_sec;
        ct->m_msec = tp.tv_nsec/1000000;
    } else {
        ct->m_sec = 0;
        ct->m_msec = 0;
    }
}

void MiscTool::clock2Time(ClockTime* tm, const ClockTime* clk) {
    tm->m_sec = clk->m_sec + g_origin_tm.m_sec;
    tm->m_msec = clk->m_msec + g_origin_tm.m_msec;
    if (1000 <= tm->m_msec) {
        tm->m_msec -= 1000;
        ++tm->m_sec;
    }
}

long MiscTool::diffMsec(const ClockTime* ct1, 
    const ClockTime* ct2) {
    long t1 = ct1->m_sec * 1000 + ct1->m_msec;
    long t2 = ct2->m_sec * 1000 + ct2->m_msec;

    return t1 - t2;
}

void MiscTool::pause() {
    ::pause();
}

unsigned long long MiscTool::read8Bytes(int fd) {
    unsigned long long ullVal = 0;
    int rdLen = 0;

    rdLen = read(fd, &ullVal, sizeof(ullVal));
    if (rdLen == sizeof(ullVal)) {
        return ullVal;
    } else {
        return 0UL;
    }
}

void MiscTool::writeEvent(int fd) {
    unsigned long long ullVal = 1;

    write(fd, &ullVal, sizeof(ullVal));
}

void MiscTool::writePipeEvent(int fd) {
    int wrLen = 0;
    unsigned char ch = 0;

    wrLen = write(fd, &ch, sizeof(ch));
    if (0 < wrLen || (0 > wrLen && EAGAIN == errno)) {
    } else {
        LOG_ERROR("write_pipe| ret=%d| fd=%d| error=%s|",
            wrLen, fd, strerror(errno));
    }
}

void MiscTool::readPipeEvent(int fd) {
    unsigned char ch[8] = {0};
    int rdLen = 0;

    rdLen = read(fd, ch, sizeof(ch));
    if (0 < rdLen || (0 > rdLen && EAGAIN == errno)) {
        /* ok */
    } else if (0 == rdLen) {
        /* closed write */
        LOG_ERROR("read_pipe| ret=%d| fd=%d| error=closed|",
            rdLen, fd);
    } else {
        /* error */
        LOG_ERROR("read_pipe| ret=%d| fd=%d| error=%s|",
            rdLen, fd, strerror(errno));
    }
}

int MiscTool::creatEvent() {
    int fd = -1; 

    fd = eventfd(1, EFD_NONBLOCK);
    if (0 < fd) { 
        return fd;
    } else {
        return -1;
    } 
}

int MiscTool::creatTimer(int ms) {
    int ret = 0;
    int fd = -1; 
    struct itimerspec value;

    fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (0 > fd) {
        return -1;
    }
    
    value.it_value.tv_sec = 0;
	value.it_value.tv_nsec = 1;

    if (1000 > ms) {
        value.it_interval.tv_sec = 0;
    } else {
        value.it_interval.tv_sec = ms / 1000;
        ms = ms % 1000;
    }
    
	value.it_interval.tv_nsec = ms * 1000000;
    ret = timerfd_settime(fd, 0, &value, NULL); 
    if (0 == ret) {
        return fd;
    } else {
        close(fd);
        return -1;
    } 
}

int MiscTool::creatPipes(int (*pfds)[2]) {
    int ret = 0;

    memset(pfds, 0, sizeof(*pfds));
    ret = pipe(*pfds);
    if (0 == ret) {
        fcntl((*pfds)[0], F_SETFL, O_NONBLOCK);
        fcntl((*pfds)[1], F_SETFL, O_NONBLOCK);
    } else {
        LOG_ERROR("creat_pipes| ret=%d| error=%s|",
            ret, strerror(errno));
        ret = -1;
    }

    return ret;
}

void MiscTool::armSig(int sig, PFunSig fn) {
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);

    act.sa_flags = SA_RESTART;

    if (NULL != fn) {
        act.sa_handler = fn;
    } else {
        act.sa_handler = SIG_DFL;
    }
  
    sigaction(sig, &act, NULL);
}

void MiscTool::raise(int sig) {
    ::raise(sig);
}

bool MiscTool::isCoreSig(int sig) {
    static const int g_core_signals[] = {
        SIGQUIT, SIGILL, SIGABRT, SIGFPE, 
        SIGSEGV, SIGBUS, SIGSYS
    };

    for (int i=0; i<ARR_CNT(g_core_signals); ++i) {
        if (g_core_signals[i] == sig) {
            return true;
        }
    }

    return false;
}

int MiscTool::maxSig() {
    return SIGRTMIN;
}

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
            
            LOG_ERROR("connect error");
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
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| msg=peer closed|",
                fd, max);
            
            ret = -2;
        } else if (EAGAIN == errno) {
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| msg=read empty|",
                fd, max);
            
            ret = 0;
        } else {
            LOG_DEBUG("recvTcp| fd=%d| maxlen=%d| msg=read error:%s|",
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
                fd, max, ret);
            ret = cnt;
        } else if (EAGAIN == errno) {
            LOG_DEBUG("sendTcp| fd=%d| maxlen=%d| msg=write block|",
                fd, max);
            ret = 0;
        } else {
            LOG_DEBUG("sendTcp| fd=%d| maxlen=%d| msg=write error:%s|",
                fd, max, strerror(errno));
            ret = -1;
        }
    }

    return ret;
}


