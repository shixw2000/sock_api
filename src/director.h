#ifndef __DIRECTOR_H__
#define __DIRECTOR_H__
#include"sockdata.h"
#include"isockapi.h"


struct TaskData;
class Receiver;
class Sender;
class Dealer;
class ManageCenter;
class SockProto;

class Director {
public:
    Director();
    ~Director();

    int init();
    void finish();

    void start();
    void stop();
    void wait();

    void setTimeout(unsigned rd_timeout, unsigned wr_timeout);

    int creatSvr(const char szIP[], int port, 
        ISockSvr* svr, long data2, 
        unsigned rd_thresh = 0, unsigned wr_thresh = 0);

    void sheduleCli(unsigned delay, const char szIP[], int port,
        ISockCli* base, long data2,
        unsigned rd_thresh = 0, unsigned wr_thresh = 0); 

    int creatCli(const char szIP[], int port,
        ISockCli* base, long data2, 
        unsigned rd_thresh = 0, unsigned wr_thresh = 0);

    int schedule(unsigned delay, TFunc func, long data, long data2);
    
    int sendCommCmd(EnumDir enDir, EnumSockCmd cmd, int fd); 
    int sendCmd(EnumDir enDir, NodeCmd* pCmd);
    int sendMsg(int fd, NodeMsg* pMsg);
    int dispatch(int fd, NodeMsg* pMsg); 
    
    int activate(EnumDir enDir, GenData* data);
    int notifyTimer(EnumDir enDir, unsigned tick); 
    void notifyClose(GenData* data);
    void notifyClose(int fd);
     
    void setProto(SockProto* proto); 

    void closeData(GenData* data);
    void initSock(GenData* data);
    
private:
    int regSvr(int fd, ISockSvr* svr, long data2,
        unsigned rd_thresh, unsigned wr_thresh,
        const char szIP[], int port);
    
    int regCli(int fd, ISockCli* cli, long data2,
        unsigned rd_thresh, unsigned wr_thresh,
        const char szIP[], int port); 
    
private:
    Receiver* m_receiver;
    Sender* m_sender;
    Dealer* m_dealer;
    ManageCenter* m_center; 
};

#endif

