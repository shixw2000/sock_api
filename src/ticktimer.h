#ifndef __TICKTIMER_H__
#define __TICKTIMER_H__
#include"llist.h"
#include"sockdata.h"


class Lock;

class TickTimer {
    static const int DEF_TV_BITS = 8;
    static const int DEF_TV_SIZE = 1 << DEF_TV_BITS;
    static const int DEF_TV_MASK = DEF_TV_SIZE - 1;
    
public:
    TickTimer();
    ~TickTimer(); 

    void startTimer(TimerObj* ele, unsigned delay,
        unsigned interval = 0);

    void restartTimer(TimerObj* ele, unsigned delay,
        unsigned interval = 0);

    void stopTimer(TimerObj* ele);
    
    void run(unsigned tick);
    
    unsigned now() const {
        return m_now;
    } 

    void setLock(Lock* lock);
    
    static void initObj(TimerObj* ele); 
    
    static void setTimerCb(TimerObj* ele, 
        TimerFunc cb, long data, long data2 = 0); 

private:    
    void tick(HRoot* list);
    void _schedule(TimerObj* ele, unsigned delay);
    
    void cascade(HRoot tv[], int index, int level);
    
    void doTimers(HRoot* list);

    inline int index(unsigned time, int level) {
        return (time >> (DEF_TV_BITS * level)) & DEF_TV_MASK;
    }

    inline unsigned remain(unsigned time, int level) {
        int mask = (1 << (DEF_TV_BITS * level)) -1;

        return time & mask;
    }

    void lock();
    void unlock();
    
private:
    Lock* m_lock;
    unsigned m_now;
    unsigned m_tick;
    HRoot m_tv1[DEF_TV_SIZE];
    HRoot m_tv2[DEF_TV_SIZE];
    HRoot m_tv3[DEF_TV_SIZE];
};

#endif

