#ifndef __TIMERMNG_H__
#define __TIMERMNG_H__
#include"llist.h"
#include"shareheader.h"


struct TimerObj;
class TickTimer;
class Lock;

/* here the time unit is sec */
class TimerMng {
    static const int MAX_TIMER_SIZE = 0x40000;

public:
    TimerMng();
    ~TimerMng(); 

    int init();
    void finish();

    unsigned now() const;

    void startTimer(TimerObj* ele, unsigned delay_sec,
        unsigned interval_sec = 0);

    void restartTimer(TimerObj* ele, unsigned delay_sec,
        unsigned interval_sec = 0);
    
    void stopTimer(TimerObj* ele);

    void setParam(TimerObj* ele, TimerFunc cb, 
        long data, long data2 = 0);
    
    TimerObj* allocTimer();
    void freeTimer(TimerObj*);

    void run();
    
private:
    void lock();
    void unlock(); 
    
private:
    Lock* m_lock; 
    TimerObj* m_timer_objs;
    TickTimer* m_timer;
    Queue m_timer_pool;
};

#endif

