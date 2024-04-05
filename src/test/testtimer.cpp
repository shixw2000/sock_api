#include"../ticktimer.h"
#include"../misc.h"


static void f1(long arg, long arg2) {
    TickTimer* timer = (TickTimer*)arg;
    TimerObj* obj = (TimerObj*)arg2;
    
    timer->schedule(obj, 1000);
    LOG_INFO("timer 1| tick=%u|", timer->now());
}

static void f2(long arg, long arg2) {
    TickTimer* timer = (TickTimer*)arg;
    TimerObj* obj = (TimerObj*)arg2;

    timer->schedule(obj, 10000);
    LOG_INFO("timer 2| tick=%u|", timer->now());
}

void testTimer() {
    TickTimer* timer = NULL;
    TimerObj obj[3];

    timer = new TickTimer;

    for (int i=0; i<3; ++i) {
        TickTimer::initObj(&obj[i]); 
    }

    TickTimer::setTimerCb(&obj[0], &f1, (long)timer, (long)&obj[0]);
    TickTimer::setTimerCb(&obj[1], &f2, (long)timer, (long)&obj[1]);
    
    timer->schedule(&obj[0], 0);
    timer->schedule(&obj[1], 10);

    for (int i=0; i<10000000; ++i) {
        timer->run(1);
    }

    LOG_INFO("end here.");
    
    MiscTool::pause();
} 

