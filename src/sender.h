#ifndef __SENDER_H__
#define __SENDER_H__
#include<poll.h>
#include"sockdata.h"
#include"cthread.h"


struct pollfd;
struct TimerObj;
class ManageCenter;
class Director;
class TickTimer;
class Lock;

class Sender: public CThread {
    typedef void (Sender::*PWrFunc)(GenData* data);
    
public:
    Sender(ManageCenter* center, Director* director);
    ~Sender();

    int init();
    void finish();
    
    GenData* find(int fd) const;
    bool exists(int fd) const;
    
    int addCmd(NodeMsg* pCmd);
    int sendMsg(int fd, NodeMsg* pMsg); 

    int notifyTimer(unsigned tick); 
    int activate(GenData* data);

private:
    void run();
    
    void doTasks();
    void consume();
    bool wait();

    void lock();
    void unlock();

    void signal();

    void setStat(GenData* data, int stat);

    bool queue(GenData* data);
    bool _queue(GenData* data, int expectStat);
    int _activate(GenData* data, int stat);
    void detach(GenData* data, int stat);

    void flowCtlCb(GenData* data);
    void flowCtl(GenData* data, unsigned total);
    
    void dealConnCb(GenData* data);
    void dealTimeoutCb(GenData* data);
    void addFlashTimeout(GenData* data, 
        int event, bool force = false);
    
    void dealRunQue(LList* list);
    void callback(GenData* data);
    void writeDefault(GenData* data);
    void writeSock(GenData* data);
    void writeConnector(GenData* data); 

    void dealCmds(LList* list); 
    void procCmd(NodeMsg* base);
    void cmdAddFd(NodeMsg* base);
    void cmdRemoveFd(NodeMsg* base); 

    void cbTimer1Sec();
    void startTimer1Sec();
    static bool sendSecCb(long data1, long, TimerObj*);
    static bool sendTimeoutCb(long p1, 
        long p2, TimerObj* obj);

    void writeMsgQue(GenData* data, LList* queue,
        unsigned now, unsigned max);

private:
    static PWrFunc m_func[ENUM_WR_END];
    LList m_run_queue;
    LList m_wait_queue;
    LList m_cmd_queue;
    bool m_busy;
    unsigned m_tick;
    unsigned m_now_sec;
    Lock* m_lock;
    struct pollfd* m_pfds;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
    TimerObj* m_1sec_obj;
    int m_ev_fd[2];
};

#endif

