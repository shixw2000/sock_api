#include"lock.h"
#include"ticktimer.h"
#include"sockdata.h"
#include"cache.h"
#include"timermng.h"


TimerMng::TimerMng() {
    initQue(&m_timer_pool);
    m_lock = NULL;
    m_timer_objs = NULL;
    m_timer = NULL;
}

TimerMng::~TimerMng() {
}

int TimerMng::init() {
    int ret = 0; 
    
    m_lock = new SpinLock; 
    m_timer = new TickTimer;

    m_timer->setLock(m_lock);

    m_timer_objs = (TimerObj*)CacheUtil::callocAlign(
        MAX_TIMER_SIZE, sizeof(TimerObj));
    
    creatQue(&m_timer_pool, MAX_TIMER_SIZE);

    for (int i=0; i<MAX_TIMER_SIZE; ++i) {
        push(&m_timer_pool, &m_timer_objs[i]);
    }

    return ret;
}

void TimerMng::finish() {
    finishQue(&m_timer_pool); 

    if (NULL != m_timer_objs) {
        CacheUtil::freeAlign(m_timer_objs);

        m_timer_objs = NULL;
    }

    if (NULL != m_timer) {
        delete m_timer;
        m_timer = NULL;
    }

    if (NULL != m_lock) {
        delete m_lock;
        m_lock = NULL;
    }
}

void TimerMng::lock() {
    m_lock->lock();
}

void TimerMng::unlock() {
    m_lock->unlock();
}

unsigned TimerMng::now() const {
    return m_timer->now();
}

void TimerMng::run() { 
    m_timer->run(1);
}

TimerObj* TimerMng::allocTimer() {
    bool bOk = false;
    void* ele = NULL;
    TimerObj* obj = NULL;
    
    lock();
    bOk = pop(&m_timer_pool, &ele);
    unlock();

    if (bOk) {
        obj = reinterpret_cast<TimerObj*>(ele);
        TickTimer::initObj(obj);
    }

    return obj;
}

void TimerMng::freeTimer(TimerObj* obj) {
    if (NULL != obj) { 
        m_timer->stopTimer(obj);
        
        lock(); 
        push(&m_timer_pool, obj);
        unlock();
    }
} 

void TimerMng::startTimer(TimerObj* ele, unsigned delay_sec,
    unsigned interval_sec) { 
    if (NULL != ele) {
        m_timer->startTimer(ele, delay_sec, interval_sec);
    }
}

void TimerMng::restartTimer(TimerObj* ele, unsigned delay_sec,
    unsigned interval_sec) { 
    if (NULL != ele) {
        m_timer->restartTimer(ele, delay_sec, interval_sec);
    }
}

void TimerMng::stopTimer(TimerObj* ele) {
    if (NULL != ele) {
        m_timer->stopTimer(ele);
    }
}

void TimerMng::setParam(TimerObj* ele, TimerFunc cb, 
    long data, long data2) {
    if (NULL != ele) {
        TickTimer::setTimerCb(ele, cb, data, data2);
    }
}

