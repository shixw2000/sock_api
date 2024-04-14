#ifndef __SOCKFRAME_H__
#define __SOCKFRAME_H__
#include"isockapi.h"


typedef void (*TFunc)(long, long);

class SockFrame {
private:
    struct _intern;
    
    SockFrame();
    ~SockFrame();

    int init();
    void finish();

public:
    static SockFrame* instance();
    static void destroy(SockFrame*); 

    void start();
    void stop();
    void wait();

    /* unit: seconds */
    void setTimeout(unsigned rd_timeout, 
        unsigned wr_timeout);

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

    int creatSvr(const char szIP[], int port, 
        ISockSvr* svr, long data2);

    int sheduleCli(unsigned delay, const char szIP[], int port,
        ISockCli* base, long data2); 

    int creatCli(const char szIP[], int port,
        ISockCli* base, long data2);

    int schedule(unsigned delay, TFunc func, long data, long data2);

private:
    _intern* m_intern;
};

extern void armSigs();

#endif

