#ifndef __SHAREHEADER_H__
#define __SHAREHEADER_H__
#include<sys/uio.h>
#include<cassert>
#include"clog.h"


#ifndef NULL
#define NULL 0
#endif

#define OFFSET(type, mem) ((unsigned long)(&((type*)0)->mem))
#define CONTAINER(ptr, type, mem) ({\
    const typeof(((type*)0)->mem)* _mptr = (ptr); \
    (type*)((char*)_mptr - OFFSET(type, mem)); })

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define CAS(ptr,test,val) __sync_bool_compare_and_swap((ptr), (test), (val))
#define CMPXCHG(ptr,test,val) __sync_val_compare_and_swap((ptr), (test), (val))
#define ATOMIC_SET(ptr,val) __sync_lock_test_and_set((ptr), (val))
#define ATOMIC_CLEAR(ptr) __sync_lock_release(ptr)
#define ATOMIC_INC_FETCH(ptr) __sync_add_and_fetch((ptr), 1)
#define ATOMIC_DEC_FETCH(ptr) __sync_sub_and_fetch((ptr), 1)

#define ARR_CNT(x) (int)(sizeof(x)/sizeof((x)[0]))
#define INIT_CLOCK_TIME {0,0}

static const char DEF_BUILD_VER[] = __BUILD_VER__;
static const int MAX_FD_NUM = 10000;
static const int MAX_BACKLOG_NUM = 10000;
static const unsigned DEF_MSG_VER = 0x0814;
static const unsigned DEF_TIMEOUT_SEC = 60;
static const unsigned KILO = 1024;
static const int DEF_IP_SIZE = 64;
static const int MAX_ADDR_SIZE = 256; 


struct NodeMsg;
struct Cache; 

typedef void (*PFree)(char*);
typedef bool (*TimerFunc)(long, long);
typedef void (*PFunSig)(int);


enum EnumSockRet {
    ENUM_SOCK_RET_OTHER = -4,
    ENUM_SOCK_RET_PARSED = -3,
    ENUM_SOCK_RET_CLOSED = -2,
    ENUM_SOCK_RET_ERR = -1,
    ENUM_SOCK_RET_OK = 0,
    ENUM_SOCK_RET_BLOCK = 1,
    ENUM_SOCK_RET_LIMIT,
}; 

enum EnumResType {
    ENUM_RES_CORE = 0,
    ENUM_RES_CPU,
    ENUM_RES_DATA,
    ENUM_RES_FILE_SIZE,
    ENUM_RES_MSG_QUEUE,
    ENUM_RES_NOFILE,
    ENUM_RES_NPROC,
    ENUM_RES_RSS,
    ENUM_RES_SIG_PENDING,
    ENUM_RES_STACK,

    ENUM_RES_END
};

struct ClockTime {
    unsigned m_sec;
    unsigned m_msec;
}; 

struct SockName {
    int m_port;
    char m_ip[DEF_IP_SIZE];
};

struct SockAddr {
    int m_addrLen;
    char m_addr[MAX_ADDR_SIZE];
};

struct AccptOption {
    long m_extra;
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    bool m_delay;
};

struct ConnOption {
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    bool m_delay;
};

struct Buffer {
    Cache* m_cache;
    int m_size;
    int m_pos;
};

#endif

