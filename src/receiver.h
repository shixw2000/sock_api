#ifndef __RECEIVER_H__
#define __RECEIVER_H__
#include<poll.h>
#include"sockdata.h"
#include"cthread.h"


struct pollfd;
struct TimerObj;
class ManageCenter;
class Director;
class TickTimer;
class Lock;

class Receiver : public CThread { 
    typedef void (Receiver::*PRdFunc)(GenData* data);
    
public:
    Receiver(ManageCenter* center, Director* director);
    ~Receiver();

    int init();
    void finish();

    GenData* find(int fd) const;
    bool exists(int fd) const;
    
    int addCmd(NodeMsg* pCmd);
    int activate(GenData* data);

private: 
    void run();

    void doTasks();
    bool wait();
    void consume();

    void lock();
    void unlock();

    void signal(); 
    
    void setStat(GenData* data, int stat);
    void _disableFd(GenData* data);
    void _enableFd(GenData* data);
    
    bool queue(GenData* data);
    bool _queue(GenData* data, int expectStat);
    int _activate(GenData* data, int stat);
    void detach(GenData* data, int stat);

    void flowCtlCb(GenData* data);
    void dealTimeoutCb(GenData* data);
    void addFlashTimeout(GenData* data,
        bool force = false);

    void dealRunQue(LList* list); 
    void callback(GenData* data);
    void readDefault(GenData* data);
    void readSock(GenData* data);
    void readListener(GenData* data);

    void dealCmds(LList* list); 
    void procCmd(NodeMsg* base);
    void cmdAddFd(NodeMsg* base);
    void cmdRemoveFd(NodeMsg* base);

    void _AddFd(NodeMsg* base, bool delay);
    void cmdDelayFd(NodeMsg* base);
    void cmdUndelayFd(NodeMsg* base);

    void flowCtl(GenData* data, unsigned total);

    void cbTimer1Sec();
    void startTimer1Sec();
    static bool recvSecCb(long, long, TimerObj*);
    static bool recvTimeoutCb(long, long, TimerObj*);

private:
    static PRdFunc m_func[ENUM_RD_END];
    LList m_run_queue;
    LList m_cmd_queue;
    bool m_busy;
    unsigned m_tick;
    Lock* m_lock;
    struct pollfd* m_pfds;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
    TimerObj* m_1sec_obj;
    unsigned m_now_sec;
    int m_size;
    int m_ev_fd[2];
    int m_timer_fd; 
};


#endif

