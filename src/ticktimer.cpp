#include<cstring>
#include<cstdlib>
#include"ticktimer.h"
#include"shareheader.h"
#include"cache.h"
#include"misc.h"
#include"lock.h"


TickTimer::TickTimer() {
    m_lock = NULL;
    
    m_tick = 0;

    MiscTool::bzero(m_tv1, sizeof(m_tv1));
    MiscTool::bzero(m_tv2, sizeof(m_tv2));
    MiscTool::bzero(m_tv3, sizeof(m_tv3));
}

TickTimer::~TickTimer() {
}

void TickTimer::cascade(HRoot tv[], int index, int level) {
    HRoot* list = &tv[index];
    HList* pos = NULL;
    HList* n = NULL;
    TimerObj* ele = NULL;
    unsigned delay = 0;

    if (!hlistEmpty(list)) {
        for_each_hlist_safe(pos, n, list) {
            hlistDel(pos);

            ele = CONTAINER(pos, TimerObj, m_node); 
            delay = remain(ele->m_expire, level);
            
            _schedule(ele, delay);
        }
    }
} 

void TickTimer::tick(HRoot* list) {
    int slot1 = 0;
    int slot2 = 0;
    int slot3 = 0;

    ++m_tick;

    slot1 = index(m_tick, 0); 
    if (0 == slot1) {
        slot2 = index(m_tick, 1);
        cascade(m_tv2, slot2, 1);
        if (0 == slot2) {
            slot3 = index(m_tick, 2);
            cascade(m_tv3, slot3, 2);
        }
    } 

    if (!hlistEmpty(&m_tv1[slot1])) {
        hlistReplace(list, &m_tv1[slot1]); 
    }
}

void TickTimer::doTimers(HRoot* list) {
    TimerObj* ele = NULL;
    HList* first = NULL;
    bool bOk = false; 
    
    /* just here, need do like this for the most safe rule */
    while (!hlistEmpty(list)) {
        first = hlistFirst(list);
        hlistDel(first); 
        
        unlock();

        /* do business out of lock */
        ele = CONTAINER(first, TimerObj, m_node); 
        if (NULL != ele->func) {
            bOk = ele->func(ele->m_data, ele->m_data2);
        } else {
            bOk = false;
        }
        lock(); 
        
        if (bOk && 0 < ele->m_interval) {
            _schedule(ele, ele->m_interval);
        }
    } 
}

void TickTimer::run(unsigned tm) {
    HRoot list = INIT_HLIST_HEAD;
  
    lock();
    
    m_now += tm;
    tick(&list);

    if (!hlistEmpty(&list)) {
        doTimers(&list);
    }
    
    unlock();
}

void TickTimer::startTimer(TimerObj* ele, 
    unsigned delay, unsigned interval) { 
    if (0 == delay) {
        delay = 1;
    }

    lock();
    ele->m_interval = interval;
    _schedule(ele, delay);
    unlock();
}

void TickTimer::restartTimer(TimerObj* ele, 
    unsigned delay, unsigned interval) { 
    if (0 == delay) {
        delay = 1;
    }

    lock();
    if (!hlistUnhashed(&ele->m_node)) {
        hlistDel(&ele->m_node);
    }
    
    ele->m_interval = interval;
    _schedule(ele, delay);
    unlock();
}

void TickTimer::_schedule(TimerObj* ele, unsigned delay) {  
    int slot = 0;
    unsigned epoch = m_tick + delay;

    ele->m_expire = epoch;
    if (delay < (1 << DEF_TV_BITS)) {
        slot = index(epoch, 0);

        hlistAdd(&m_tv1[slot], &ele->m_node);
    } else if (delay < (1 << (2 * DEF_TV_BITS))) {
        slot = index(epoch, 1);
        
        hlistAdd(&m_tv2[slot], &ele->m_node);
    } else if (delay < (1 << (3 * DEF_TV_BITS))) {
        slot = index(epoch, 2);
        
        hlistAdd(&m_tv3[slot], &ele->m_node);
    } else {
        /* overflow, then just use the lowest 8 bits */
        slot = index(epoch, 0);
        
        hlistAdd(&m_tv1[slot], &ele->m_node);
    } 
}

void TickTimer::initObj(TimerObj* ele) {
    MiscTool::bzero(ele, sizeof(TimerObj));
}

void TickTimer::setTimerCb(TimerObj* ele, 
    TimerFunc func, long data, long data2) {
    ele->func = func;
    ele->m_data = data;
    ele->m_data2 = data2;
}

void TickTimer::stopTimer(TimerObj* ele) {
    lock();
    
    if (!hlistUnhashed(&ele->m_node)) {
        hlistDel(&ele->m_node);
    }
    
    unlock();
}

void TickTimer::setLock(Lock* lock) {
    m_lock = lock;
}

void TickTimer::lock() {
    if (NULL != m_lock) {
        m_lock->lock();
    }
}

void TickTimer::unlock() {
    if (NULL != m_lock) {
        m_lock->unlock();
    }
}

