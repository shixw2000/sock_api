#ifndef __MISC_H__
#define __MISC_H__
#include"shareheader.h" 


class MiscTool {
public:
    static void bzero(void* dst, int size);
    static void bcopy(void* dst, const void* src, int size);
    
    static int strLen(const char* src, int max);
    static int strCpy(void* dst, const char* src, int max);
    static int strPrint(void* dst, int max, const char format[], ...);
    
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

    static void sleepSec(int sec);

    static int mkDir(const char dir[]);

    static void armSig(int sig, PFunSig fn);
    static void raise(int sig);
    static bool isCoreSig(int sig);
    static int maxSig();

    static int setRlimit(EnumResType type, 
        const unsigned long* softV, 
        const unsigned long* hardV);

    static int getRlimit(EnumResType type, 
        unsigned long* softV, 
        unsigned long* hardV);
}; 

#endif

