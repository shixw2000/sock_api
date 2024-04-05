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
    
    int addCmd(NodeCmd* pCmd);
    int activate(GenData* data); 

    void flowCtlCallback(GenData* data);


private: 
    void run();

    void doTasks();
    bool wait();
    void consume();

    void lock();
    void unlock();

    void signal(); 
    
    void setStat(GenData* data, int stat);
    void lockSet(GenData* data, int stat);
    void _disableFd(GenData* data);
    void _enableFd(GenData* data);
    
    bool queue(GenData* data);
    bool _queue(GenData* data, int expectStat);
    int _activate(GenData* data, int expectStat);

    void dealFlashTimeout(unsigned now, LList* list);
    void addFlashTimeout(unsigned now, 
        LList* list, GenData* data);

    void dealRunQue(LList* list); 
    void callback(GenData* data);
    void readDefault(GenData* data);
    void readSock(GenData* data);
    void readListener(GenData* data);
    void readTimer(GenData* data);

    void dealCmds(LList* list); 
    void procCmd(NodeCmd* base);
    void cmdAddFd(NodeCmd* base);
    void cmdRemoveFd(NodeCmd* base);

    void updateBytes(GenData* data, unsigned now);
    void flowCtl(GenData* data, unsigned total);

    void cbTimer1Sec();
    void startTimer1Sec();
    static void Recv1SecCb(long data1, long);

    void postSendData(unsigned now, GenData* data,
        int stat, unsigned max, unsigned total);

private:
    static PRdFunc m_func[ENUM_RD_END];
    LList m_run_queue;
    LList m_cmd_queue;
    LList m_time_flash_queue;
    bool m_busy;
    Lock* m_lock;
    struct pollfd* m_pfds;
    ManageCenter* m_center;
    Director* m_director;
    TickTimer* m_timer;
    TimerObj* m_1sec_obj;
    int m_size;
    int m_ev_fd;
};


#endif

