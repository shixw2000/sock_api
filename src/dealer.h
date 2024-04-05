#ifndef __DEALER_H__
#define __DEALER_H__
#include"sockdata.h"
#include"cthread.h"


class ManageCenter;
class Director;
class TickTimer;
class MutexCond;

class Dealer : public CThread {
    typedef void (Dealer::*PDealFunc)(GenData* data);
    
public:
    Dealer(ManageCenter* center, Director* director);
    ~Dealer();
    
    int init();
    void finish();

    GenData* find(int fd) const;
    bool exists(int fd) const;
    
    int addCmd(NodeCmd* pCmd);
    int dispatch(int fd, NodeMsg* pMsg);
    int activate(GenData* data); 
    int notifyTimer(GenData* data, unsigned tick);

private:
    void run();
    
    void doTasks();

    void lock();
    void unlock();

    void wait();
    void signal();

    void setStat(GenData* data, int stat);
    void lockSet(GenData* data, int stat);
    bool _queue(GenData* data, int expectStat);
    int _activate(GenData* data, int expectStat);

    void dealRunQue(LList* list);
    void callback(GenData* data);
    void procDefault(GenData* data);
    void procMsg(GenData* data);
    void procConnector(GenData* data);
    void procListener(GenData* listenData);
    void procTimer(GenData* data);
    
    void dealCmds(LList* list); 
    void procCmd(NodeCmd* base);
    void cmdRemoveFd(NodeCmd* base); 
    void cmdConnReport(NodeCmd* base);
    void cmdSchedTask(NodeCmd* base);

private:
    static PDealFunc m_func[ENUM_DEAL_END];
    LList m_run_queue;
    LList m_cmd_queue;
    bool m_busy;
    MutexCond* m_lock;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
};

#endif

