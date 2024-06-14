#ifndef __DIRECTOR_H__
#define __DIRECTOR_H__
#include"sockdata.h"
#include"isockapi.h"


class Receiver;
class Sender;
class Dealer;
class ManageCenter;
class TimerMng;

class Director {
public:
    Director();
    ~Director();

    int init(int cap);
    void finish();

    void start();
    void stop();
    void wait(); 

    int creatSvr(const char szIP[], int port, 
        ISockSvr* svr, long data2);

    int sheduleCli(unsigned delay, const char szIP[], int port,
        ISockCli* base, long data2); 

    int creatCli(const char szIP[], int port,
        ISockCli* base, long data2); 

    int regUdp(int fd, ISockBase* base, long data2);

    int regSvr(int fd, ISockSvr* svr, long data2,
        const char szIP[], int port);
    
    int regCli(int fd, ISockCli* cli, long data2,
        const char szIP[], int port); 

    int regSock(int newFd, GenData* parent, const SockAddr* addr);

    void beginSock(GenData* data);

    void activateSock(GenData* data, unsigned rd_thresh, 
        unsigned wr_thresh, bool delay);
    
    int sendCommCmd(EnumDir enDir, EnumSockCmd cmd, int fd); 
    int sendCmd(EnumDir enDir, NodeMsg* pCmd);
    int sendMsg(int fd, NodeMsg* pMsg);
    int dispatch(int fd, NodeMsg* pMsg); 
    
    int activate(EnumDir enDir, GenData* data);
    int notifyTimer(EnumDir enDir, unsigned tick); 
     
    int getAddr(int fd, int* pPort, char ip[], int max);
    long getExtra(int fd);

    void undelayRead(int fd);

    /* unit: kilo bytes */
    void getSpeed(int fd, unsigned& rd_thresh,
        unsigned& wr_thresh);
    
    /* unit: kilo bytes */
    void setSpeed(int fd, unsigned rd_thresh, 
        unsigned wr_thresh);

    /* seconds */
    void setTimeout(unsigned rd_timeout, unsigned wr_timeout);

    /* seconds */
    void setMaxRdTimeout(int fd, unsigned timeout);
    void setMaxWrTimeout(int fd, unsigned timeout);

    void stopSock(GenData* data);

    void notifyClose(GenData* data);

    void closeData(int fd);

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
    
    void runPerSec();

private: 
    NodeMsg* creatCmdComm(EnumSockCmd cmd, int fd);

private:
    Receiver* m_receiver;
    Sender* m_sender;
    Dealer* m_dealer;
    ManageCenter* m_center;
    TimerMng* m_timer_mng;
    int m_fd_max;
};

#endif

