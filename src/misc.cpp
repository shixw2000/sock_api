#include<unistd.h>
#include<cstdio>
#include<cstring>
#include<sys/time.h>
#include<sys/resource.h>
#include<time.h>
#include<stdarg.h>
#include<cstdlib>
#include<signal.h>
#include<sys/types.h>
#include<sys/syscall.h>
#include<sys/socket.h>
#include<sys/eventfd.h>
#include<sys/timerfd.h>
#include<arpa/inet.h> 
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/stat.h>
#include"misc.h"
#include"clog.h"


#define __INIT_RUN__ __attribute__((unused, constructor))
#define __END_RUN__ __attribute__((unused, destructor))

struct _Global {
    ClockTime m_origin_tm;
    int m_pid;
    int m_dev_fd; 
};

static _Global g_global = {
    INIT_CLOCK_TIME,
    0,
    0,
};

static int _getDevHd() {
    int fd = -1;
    
    fd = open("/dev/urandom", O_RDONLY);
    if (0 < fd) { 
        MiscTool::setNonBlock(fd);
        return fd;
    } else {
        fd = open("/dev/random", O_RDONLY);
        if (0 < fd) {
            MiscTool::setNonBlock(fd);
            return fd;
        } else {
            return 0;
        }
    }
} 

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

static void _initTime() {
    ClockTime ct1;

    _getTime(&g_global.m_origin_tm);
    MiscTool::getClock(&ct1);

    if (g_global.m_origin_tm.m_msec >= ct1.m_msec) {
        g_global.m_origin_tm.m_msec -= ct1.m_msec;
        g_global.m_origin_tm.m_sec -= ct1.m_sec;
    } else {
        g_global.m_origin_tm.m_msec += 1000 - ct1.m_msec;
        g_global.m_origin_tm.m_sec -= ct1.m_sec + 1;
    }
} 

static __INIT_RUN__ void initLib() {
    _initTime();

    g_global.m_dev_fd = _getDevHd();
    srand(MiscTool::getTimeSec());
} 

static void __END_RUN__ finishLib() {
    if (0 < g_global.m_dev_fd) {
        close(g_global.m_dev_fd);
        g_global.m_dev_fd = 0;
    }

    g_global.m_pid = 0;
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
    tm->m_sec = clk->m_sec + g_global.m_origin_tm.m_sec;
    tm->m_msec = clk->m_msec + g_global.m_origin_tm.m_msec;
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

void MiscTool::getTimeMs(unsigned long* pn) { 
    ClockTime ct = {0, 0};
    
    getTime(&ct);

    *pn = ct.m_sec;
    *pn = *pn * 1000 + ct.m_msec;
}

unsigned MiscTool::getTimeSec() {
    ClockTime ct = {0, 0};
    
    getTime(&ct);

    return ct.m_sec;
}

void MiscTool::setNonBlock(int fd) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void MiscTool::pause() {
    ::pause();
}

int MiscTool::getTid() {
    int id = 0;

    id = syscall(SYS_gettid);
    return id;
}

int MiscTool::getPid() {
    int id = 0;

    id = (int)getpid();
    
    return id;
}

int MiscTool::getFastPid() {
    if (0 != g_global.m_pid) {
        return g_global.m_pid;
    } else {
        g_global.m_pid = getPid();
        return g_global.m_pid;
    } 
}

int MiscTool::mkDir(const char dir[]) {
    int ret = 0;

    ret = mkdir(dir, 0755);
    if (0 == ret || EEXIST == errno) {
        return 0;
    } else {
        return -1;
    }
}

void MiscTool::armSig(int sig, PFunSig fn) {
    struct sigaction act;

    MiscTool::bzero(&act, sizeof(act));
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

    MiscTool::bzero(pfds, sizeof(*pfds));
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

void MiscTool::getRand(void* buf, int len) {
    int* pn = (int*)buf; 
    int r = 0;
    int n = 0;
    
    if (0 < len) {
        if (0 < g_global.m_dev_fd) {
            read(g_global.m_dev_fd, buf, len);
        } else {
            r = (len >> 2);
            for (int i=0; i<r; ++i, ++pn) {
                *pn = rand();
            }

            r = (len & 0x3);
            if (0 < r) {
                n = rand();
                MiscTool::bcopy(pn, &n, r);
            }
        } 
    }
} 

void MiscTool::sleepSec(int sec) {
    sleep(sec);
}

void MiscTool::bzero(void* dst, int size) {
    if (0 < size) {
        memset(dst, 0, size);
    }
}

void MiscTool::bcopy(void* dst,
    const void* src, int size) {
    if (0 < size) {
        memcpy(dst, src, size);
    }
}

int MiscTool::strLen(const char* src, int max) {
    int len = 0;

    if (NULL != src) {
        len = (int)strnlen(src, max);
    }
    
    return len;
}

int MiscTool::strCpy(void* dst, const char* src, int max) {
    char* psz = (char*)dst;
    int len = 0;

    if (0 < max) {
        len = strLen(src, max-1);
        if (0 < len) {
            memcpy(dst, src, len);
            psz[len] = '\0';
        } else {
            psz[len] = '\0';
        }
    }

    return len;
}

int MiscTool::strPrint(void* dst, int max, const char format[], ...) {
    char* psz = (char*)dst;
    va_list ap;
    int len = 0;

    if (0 < max) {
        va_start(ap, format);
        len = vsnprintf(psz, max, format, ap);
        va_end(ap);
        if (0 < len && len < max) {
            /* ok */
        } else if (len >= max) {
            len = max - 1; 
        } else {
            len = 0;
        }

        psz[len] = '\0';
    }

    return len;
}

static const int DEF_SYS_RES[ENUM_RES_END] = {
    RLIMIT_CORE,
    RLIMIT_CPU,
    RLIMIT_DATA,
    RLIMIT_FSIZE,
    RLIMIT_MSGQUEUE,
    RLIMIT_NOFILE,
    RLIMIT_NPROC,
    RLIMIT_RSS,
    RLIMIT_SIGPENDING,
    RLIMIT_STACK,
};

int MiscTool::setRlimit(EnumResType type, 
    const unsigned long* softV, 
    const unsigned long* hardV) {
    int ret = 0;
    struct rlimit rlim;

    bzero(&rlim, sizeof(rlim));
    
    if (NULL != softV) {
        rlim.rlim_cur = *softV;
    }

    if (NULL != hardV) {
        rlim.rlim_max = *hardV;
    }
    
    ret = setrlimit(DEF_SYS_RES[type], &rlim);
    if (0 == ret) {
        LOG_INFO("set_rlimit| type=%d| softv=%lu|"
            " hardv=%lu| msg=ok|",
            type, rlim.rlim_cur, 
            rlim.rlim_max);
    } else {
        LOG_WARN("set_rlimit| type=%d| softv=%lu|"
            " hardv=%lu| error=%d:%s|",
            type, rlim.rlim_cur, 
            rlim.rlim_max,
            errno, strerror(errno));
        ret = -1;
    }
    
    return ret;
}

int MiscTool::getRlimit(EnumResType type, 
    unsigned long* softV, unsigned long* hardV) {
    int ret = 0;
    struct rlimit rlim;

    bzero(&rlim, sizeof(rlim));
    ret = getrlimit(DEF_SYS_RES[type], &rlim);
    if (0 == ret) {
        LOG_INFO("get_rlimit| type=%d| softv=%lu|"
            " hardv=%lu| msg=ok|",
            type, rlim.rlim_cur, 
            rlim.rlim_max);
    } else {
        LOG_WARN("get_rlimit| type=%d| error=%d:%s|",
            type, errno, strerror(errno));
        ret = -1;
    }

    if (NULL != softV) {
        *softV = rlim.rlim_cur;
    }

    if (NULL != hardV) {
        *hardV = rlim.rlim_max;
    }

    return ret;
}

