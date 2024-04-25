#ifndef __SHAREHEADER_H__
#define __SHAREHEADER_H__
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

static const unsigned DEF_MSG_VER = 0x0814;
static const unsigned DEF_TIMEOUT_SEC = 60;
static const unsigned KILO = 1024;

static const char DEF_BUILD_VER[] = __BUILD_VER__;

#endif

