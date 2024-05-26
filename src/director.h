#ifndef __DIRECTOR_H__
#define __DIRECTOR_H__
#include"sockdata.h"
#include"isockapi.h"


struct TaskData;
class Receiver;
class Sender;
class Dealer;
class ManageCenter;

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

    void beginSock(GenData* data, 
        unsigned rd_thresh, 
        unsigned wr_thresh, bool delay);

    void closeData(GenData* data);

    int schedule(unsigned delay, unsigned interval,
        TimerFunc func, long data, long data2);

    void setTimerPerSec(ITimerCb* cb);
    
    int sendCommCmd(EnumDir enDir, EnumSockCmd cmd, int fd); 
    int sendCmd(EnumDir enDir, NodeMsg* pCmd);
    int sendMsg(int fd, NodeMsg* pMsg);
    int dispatch(int fd, NodeMsg* pMsg); 
    
    int activate(EnumDir enDir, GenData* data);
    int notifyTimer(EnumDir enDir, unsigned tick); 
    void notifyClose(GenData* data);
    void notifyClose(int fd);
     
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

private: 
    NodeMsg* creatCmdComm(EnumSockCmd cmd, int fd); 
    NodeMsg* creatCmdSchedTask(unsigned delay,
        unsigned interval, TimerFunc func,
        long data, long data2);
    
private:
    Receiver* m_receiver;
    Sender* m_sender;
    Dealer* m_dealer;
    ManageCenter* m_center;
    int m_fd_max;
};

#endif

