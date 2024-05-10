#ifndef __MANAGECENTER_H__
#define __MANAGECENTER_H__
#include"sockdata.h"


class Lock;
class TickTimer;
class ISockBase;

enum EnumTimerEvent {
    ENUM_TIMER_EVENT_TIMEOUT = 100,
    ENUM_TIMER_EVENT_CONN_TIMEOUT,
    ENUM_TIMER_EVENT_FLOWCTL,

    ENUM_TIMER_EVENT_END
};

class ManageCenter { 
public:
    ManageCenter();
    ~ManageCenter();

    int init();
    void finish(); 

    int capacity() const {
        return m_cap;
    }

    TimerObj* allocTimer();
    void freeTimer(TimerObj*);

    bool updateExpire(EnumDir enDir, GenData* data,
        unsigned now, bool force);

    void recordBytes(EnumDir enDir, GenData* data, 
        unsigned now, unsigned total);
    
    void clearBytes(EnumDir enDir, 
        GenData* data, unsigned now); 
    
    unsigned calcThresh(EnumDir enDir, 
        GenData* data, unsigned now);

    int getFd(GenData* ele) const;

    int getRdIndex(GenData* ele) const;
    void setRdIndex(GenData* ele, int index); 

    int getStat(EnumDir enDir, GenData* data) const;
    void setStat(EnumDir enDir, GenData* data, int stat);
    void setCb(EnumDir enDir, GenData* data, int cb);
    int getCb(EnumDir enDir, GenData* data) const;

    void setConnRes(GenData* data, int res); 
    
    long getExtra(const GenData* data);

    /* unit: bytes */
    unsigned getRdThresh(GenData* data) const;

    /* unit: bytes */
    unsigned getWrThresh(GenData* data) const; 
    
    /* unit: bytes */
    void setSpeed(GenData* data, unsigned rd_thresh, unsigned wr_thresh);
    
    /* unit: ticks */
    void setTimeout(unsigned rd_timeout, unsigned wr_timeout);

    /* unit: ticks */
    void setMaxRdTimeout(GenData* data, unsigned timeout);
    void setMaxWrTimeout(GenData* data, unsigned timeout);
    
    void setDefIdleTimeout(GenData* data);
    void setDefConnTimeout(GenData* data);

    void setAddr(GenData* data, const char szIP[], int port);
    int getAddr(int fd, int* port, char ip[], int max);

    void lock();
    void unlock();
    
    GenData* find(int fd) const;
    bool exists(int fd) const; 
    
    bool isClosed(GenData* data) const;
    bool markClosed(GenData* data);
    bool markClosed(int fd);

    void setData(GenData* data, ISockBase* sock, long extra);
    GenData* reg(int fd);
    void unreg(int fd); 

    NodeMsg* creatCmdComm(EnumSockCmd cmd, int fd); 
    NodeMsg* creatCmdSchedTask(unsigned delay,
        unsigned interval, TimerFunc func,
        long data, long data2);

    int writeMsg(int fd, NodeMsg* pMsg, int max);
    int writeExtraMsg(int fd, NodeMsg* pMsg, int max);
    
    int recvMsg(GenData* data, int max);
    int addMsg(EnumDir enDir, GenData* data, NodeMsg* pMsg);
    void appendQue(EnumDir enDir, LList* to, GenData* data);
    LList* getWrPrivQue(GenData* data) const;

    void addNode(EnumDir enDir, LList* root, GenData* data);
    void delNode(EnumDir enDir, GenData* data);
    GenData* fromNode(EnumDir enDir, LList* node);
    
    void enableTimer(EnumDir enDir, TickTimer* timer, 
        GenData* data, int event);
    void cancelTimer(EnumDir enDir, GenData* data);
    void setTimerParam(EnumDir enDir, GenData* data, 
        TFunc func, long param1);

    static GenData* fromTimeout(EnumDir enDir, TimerObj* obj);

    int onNewSock(GenData* parentData,
        GenData* childData, AccptOption& opt);
    int onConnect(GenData* data, ConnOption& opt);
    int onProcess(GenData* data, NodeMsg* pMsg);
    void onClose(GenData* data);

    void initSock(GenData* data);
    
private:
    GenData* _allocData();
    void _releaseData(GenData* ele);
    
    void releaseMsgQue(LList* runlist);

    void calcBytes(EnumDir enDir, GenData* data, 
        unsigned now, unsigned& total, 
        unsigned& total2, unsigned& thresh);

    void releaseRd(GenData* data);
    void releaseWr(GenData* data);
    void releaseDeal(GenData* data); 
    
private:
    const int m_cap;
    const int m_timer_cap;
    Lock* m_lock; 
    TimerObj* m_timer_objs;
    GenData* m_cache;
    GenData** m_datas;
    Queue m_timer_pool;
    Queue m_pool;
    unsigned m_conn_timeout;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    char m_buf[MAX_BUFF_SIZE];
};

#endif

