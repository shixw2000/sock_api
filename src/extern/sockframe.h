#ifndef __SOCKFRAME_H__
#define __SOCKFRAME_H__
#include"isockapi.h"
#include"shareheader.h" 


struct TimerObj;

class SockFrame {
private:
    struct _intern;
    
    SockFrame();
    ~SockFrame();

    int init(int cap);
    void finish();

    bool chkValid() const;

public:
    static SockFrame* instance(); 
    static void creat(int cap = MAX_FD_NUM);
    static void destroy(SockFrame* frame); 

    void start();
    void stop();
    void wait();

    /* unit: seconds */
    void setTimeout(unsigned rd_timeout, 
        unsigned wr_timeout);

    /* seconds */
    void setMaxRdTimeout(int fd, unsigned timeout);
    void setMaxWrTimeout(int fd, unsigned timeout);

    int getAddr(int fd, int* pPort, char ip[], int max);
    
    long getExtra(int fd);

    /* unit: kilo bytes */
    void getSpeed(int fd, unsigned& rd_thresh,
        unsigned& wr_thresh);
    
    /* unit: kilo bytes */
    void setSpeed(int fd, unsigned rd_thresh, 
        unsigned wr_thresh);
    
    int sendMsg(int fd, NodeMsg* pMsg);
    int dispatch(int fd, NodeMsg* pMsg);
    void closeData(int fd);
    void undelayRead(int fd);

    int regUdp(int fd, ISockBase* base, long data2);
    
    int creatSvr(const char szIP[], int port, 
        ISockSvr* svr, long data2);

    int sheduleCli(unsigned delay, const char szIP[], int port,
        ISockCli* base, long data2); 

    int creatCli(const char szIP[], int port,
        ISockCli* base, long data2);

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
    
private:
    _intern* m_intern;
    bool m_bValid;
};

extern void armSigs();

#endif

