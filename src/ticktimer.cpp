#include<cstring>
#include<cstdlib>
#include"ticktimer.h"
#include"shareheader.h"


TickTimer::TickTimer() {
    m_tick = 0;

    memset(m_tv1, 0, sizeof(m_tv1));
    memset(m_tv2, 0, sizeof(m_tv2));
    memset(m_tv3, 0, sizeof(m_tv3));
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

void TickTimer::tick() {
    int slot1 = 0;
    int slot2 = 0;
    int slot3 = 0;
    HRoot list = INIT_HLIST_HEAD; 

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
        hlistReplace(&list, &m_tv1[slot1]); 

        doTimers(&list);
    } 
}

void TickTimer::doTimer(TimerObj* ele) {
    bool bOk = false;
    
    bOk = ele->func(ele->m_data, ele->m_data2, ele);
    if (bOk && 0 < ele->m_interval) {
        _schedule(ele, ele->m_interval);
    }
}

void TickTimer::doTimers(HRoot* list) {
    TimerObj* ele = NULL;
    HList* first = NULL;
    
    /* just here, need do like this for the most safe rule */
    while (!hlistEmpty(list)) {
        first = hlistFirst(list);
        hlistDel(first); 

        ele = CONTAINER(first, TimerObj, m_node); 
        
        doTimer(ele);
    } 
}

void TickTimer::run(unsigned tm) {
    m_now += tm;

    tick();
}

void TickTimer::schedule(TimerObj* ele, unsigned delay, unsigned interval) { 
    if (0 == delay) {
        delay = 1;
    }
    
    ele->m_interval = interval;
    _schedule(ele, delay);
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

void TickTimer::delTimer(TimerObj* ele) {
    if (!hlistUnhashed(&ele->m_node)) {
        hlistDel(&ele->m_node);
    }
}

void TickTimer::initObj(TimerObj* ele) {
    memset(ele, 0, sizeof(TimerObj));
}

void TickTimer::setTimerCb(TimerObj* ele, 
    TFunc func, long data, long data2) {
    ele->func = func;
    ele->m_data = data;
    ele->m_data2 = data2;
}

TimerObj* TickTimer::allocObj() {
    TimerObj* obj = NULL;

    obj = (TimerObj*)malloc(sizeof(TimerObj));
    if (NULL != obj) {
        initObj(obj);
    }
    
    return obj;
}

void TickTimer::freeObj(TimerObj* obj) {
    if (NULL != obj) {
        free(obj);
    }
}

