#ifndef __DEALER_H__
#define __DEALER_H__
#include"sockdata.h"
#include"cthread.h"


class ManageCenter;
class Director;
class TickTimer;
class MutexCond;
class ITimerCb;

class Dealer : public CThread {
    typedef void (Dealer::*PDealFunc)(GenData* data);
    
public:
    Dealer(ManageCenter* center, Director* director);
    ~Dealer();
    
    int init();
    void finish();

    virtual void stop();

    GenData* find(int fd) const;
    bool exists(int fd) const;
    
    int addCmd(NodeMsg* pCmd);
    int dispatch(int fd, NodeMsg* pMsg);
    int activate(GenData* data); 
    int notifyTimer(unsigned tick);

    void setTimerPerSec(ITimerCb* cb);

    static bool dealerTimeoutCb(long p1, 
        long p2, TimerObj* obj);

private:
    void run();
    
    void doTasks();

    void lock();
    void unlock();

    void wait();
    void signal();

    void setStat(GenData* data, int stat);
    bool _queue(GenData* data, int expectStat);
    void detach(GenData* data, int stat);

    void dealRunQue(LList* list);
    void callback(GenData* data);
    void procDefault(GenData* data);
    void procMsg(GenData* data);
    void procConnector(GenData* data);
    void procListener(GenData* listenData);
    
    int dealMsgQue(GenData* data, LList* list);
    
    void dealCmds(LList* list); 
    void procCmd(NodeMsg* base);
    void cmdRemoveFd(NodeMsg* base); 
    void cmdSchedTask(NodeMsg* base); 
    
    void onAccept(GenData* listenData,
        int newFd, const SockAddr* addr);

    void cbTimer1Sec();
    void startTimer1Sec();
    void dealTimeoutCb(GenData* data);
    
    static bool dealSecCb(long data1, long, TimerObj*);

private:
    static PDealFunc m_func[ENUM_DEAL_END];
    LList m_run_queue;
    LList m_run_queue_priv;
    LList m_cmd_queue;
    bool m_busy;
    unsigned m_tick;
    MutexCond* m_lock;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
    TimerObj* m_1sec_obj;
    ITimerCb* m_timer_cb;
};

#endif

