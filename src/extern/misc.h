#ifndef __MISC_H__
#define __MISC_H__
#include"shareheader.h"


typedef void (*PFunSig)(int);

struct ClockTime {
    unsigned m_sec;
    unsigned m_msec;
};

#define INIT_CLOCK_TIME {0,0}

class MiscTool {
public:
    static void getTime(ClockTime* ct);
    
    static void getClock(ClockTime* ct);

    static void getTimeMs(unsigned long* ct);
    static unsigned getTimeSec();
    
    static void clock2Time(ClockTime* tm, 
        const ClockTime* clk);
    
    /* ct1 - ct2 */
    static long diffMsec(const ClockTime* ct1, 
        const ClockTime* ct2);

    static void getRand(void* buf, int len);
    
    static void setNonBlock(int fd);

    static unsigned long long read8Bytes(int fd);
    static void writeEvent(int fd);
    static void writePipeEvent(int fd);
    static void readPipeEvent(int fd);

    static int creatEvent();
    static int creatTimer(int ms);
    static int creatPipes(int (*)[2]); 
    
    static void pause();
    static int getTid();
    static int getPid();
    static int getFastPid();

    static int mkDir(const char dir[]);

    static void armSig(int sig, PFunSig fn);
    static void raise(int sig);
    static bool isCoreSig(int sig);
    static int maxSig();
}; 

#endif

