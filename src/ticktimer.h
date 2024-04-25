#ifndef __TICKTIMER_H__
#define __TICKTIMER_H__
#include"llist.h"
#include"sockdata.h"

class TickTimer {
    static const int DEF_TV_BITS = 8;
    static const int DEF_TV_SIZE = 1 << DEF_TV_BITS;
    static const int DEF_TV_MASK = DEF_TV_SIZE - 1;
    
public:
    TickTimer();
    ~TickTimer();

    static void delTimer(TimerObj* ele);
    
    static TimerObj* allocObj();
    static void freeObj(TimerObj* obj); 
    static void initObj(TimerObj* ele); 
    
    static void setTimerCb(TimerObj* ele, 
        TFunc cb, long data, long data2 = 0);

    void schedule(TimerObj* ele, unsigned delay = 0,
        unsigned interval = 0);

    void run(unsigned tick);
    
    unsigned now() const {
        return m_now;
    } 

private:    
    void tick();
    void _schedule(TimerObj* ele, unsigned delay);
    
    void cascade(HRoot tv[], int index, int level);
    
    void doTimers(HRoot* list);

    void doTimer(TimerObj* ele); 

    inline int index(unsigned time, int level) {
        return (time >> (DEF_TV_BITS * level)) & DEF_TV_MASK;
    }

    inline unsigned remain(unsigned time, int level) {
        int mask = (1 << (DEF_TV_BITS * level)) -1;

        return time & mask;
    }
    
private:
    unsigned m_now;
    unsigned m_tick;
    HRoot m_tv1[DEF_TV_SIZE];
    HRoot m_tv2[DEF_TV_SIZE];
    HRoot m_tv3[DEF_TV_SIZE];
};

#endif

