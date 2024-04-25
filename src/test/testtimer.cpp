#include"../ticktimer.h"
#include"misc.h"


static bool f1(long arg, long, TimerObj*) {
    TickTimer* timer = (TickTimer*)arg;
    
    LOG_INFO("timer 1| tick=%u|", timer->now());
    return true;
}

static bool f2(long arg, long, TimerObj*) {
    TickTimer* timer = (TickTimer*)arg;

    LOG_INFO("timer 2| tick=%u|", timer->now());
    return true;
}

void testTimer() {
    TickTimer* timer = NULL;
    TimerObj obj[3];

    timer = new TickTimer;

    for (int i=0; i<3; ++i) {
        TickTimer::initObj(&obj[i]); 
    }

    TickTimer::setTimerCb(&obj[0], &f1, (long)timer);
    TickTimer::setTimerCb(&obj[1], &f2, (long)timer);
    
    timer->schedule(&obj[0], 0, 1000);
    timer->schedule(&obj[1], 10, 8903);

    for (int i=0; i<10000000; ++i) {
        timer->run(1);
    }

    LOG_INFO("end here.");
    
    MiscTool::pause();
} 

