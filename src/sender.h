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
    
    int addCmd(NodeCmd* pCmd);
    int sendMsg(int fd, NodeMsg* pMsg);

    void flowCtlCallback(GenData* data);
    void flowCtl(GenData* data, unsigned total);

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

    void dealFlashConn(unsigned now, LList* list);
    void dealFlashTimeout(unsigned now, LList* list);
    void addFlash(unsigned now, LList* list, GenData* data);
    
    void dealRunQue(LList* list);
    void callback(GenData* data);
    void writeDefault(GenData* data);
    void writeSock(GenData* data);
    void writeConnector(GenData* data); 

    void dealCmds(LList* list); 
    void procCmd(NodeCmd* base);
    void cmdAddFd(NodeCmd* base);
    void cmdRemoveFd(NodeCmd* base); 

    void cbTimer1Sec();
    void startTimer1Sec();
    static void sendSecCb(long data1, long);

    void writeMsgQue(GenData* data, LList* queue,
        unsigned now, unsigned max);

private:
    static PWrFunc m_func[ENUM_WR_END];
    LList m_run_queue;
    LList m_wait_queue;
    LList m_cmd_queue;
    LList m_time_flash_queue;
    LList m_time_flash_conn;
    bool m_busy;
    unsigned m_tick;
    Lock* m_lock;
    struct pollfd* m_pfds;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
    TimerObj* m_1sec_obj;
    int m_ev_fd[2];
};

#endif

