#ifndef __MANAGECENTER_H__
#define __MANAGECENTER_H__
#include"sockdata.h"
#include"socktool.h"


class Lock;
class TickTimer;
class ISockBase;

enum EnumTimerEvent {
    ENUM_TIMER_EVENT_TIMEOUT = 100,
    ENUM_TIMER_EVENT_CONN_TIMEOUT,
    ENUM_TIMER_EVENT_FLOWCTL,
    ENUM_TIMER_EVENT_LISTENER_SLEEP,

    ENUM_TIMER_EVENT_END
};

class ManageCenter { 
public:
    explicit ManageCenter(int cap);
    ~ManageCenter();

    int init();
    void finish(); 

    int capacity() const {
        return m_cap;
    }

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

    void setData(GenData* data, ISockBase* sock, long extra); 
    
    GenData* reg(int fd);
    bool unreg(GenData* data);

    int addMsg(EnumDir enDir, GenData* data, NodeMsg* pMsg);
    
    LList* getWrQue(GenData* data);

    LList* getDealQue(GenData* data);

    void addNode(EnumDir enDir, LList* root, GenData* data);
    void delNode(EnumDir enDir, GenData* data);
    GenData* fromNode(EnumDir enDir, LList* node);
    
    void startTimer(EnumDir enDir, TickTimer* timer, 
        GenData* data, int event, unsigned delay = 0);

    void restartTimer(EnumDir enDir, TickTimer* timer, 
        GenData* data, int event, unsigned delay = 0);
    
    void cancelTimer(EnumDir enDir, TickTimer* timer,
        GenData* data);
    
    void setTimerParam(EnumDir enDir, GenData* data, 
        TimerFunc func, long param1);

    static int getEvent(EnumDir enDir, GenData* data);

    int onRecv(GenData* data, const char* buf,
        int size, const SockAddr* addr);
    
    int onNewSock(GenData* newData,
        GenData* parent, AccptOption& opt);
    int onConnect(GenData* data, ConnOption& opt);
    int onProcess(GenData* data, NodeMsg* pMsg);
    void onClose(GenData* data); 
    
private: 
    void _allocData(GenData* obj);
    void _releaseData(GenData* ele);
    
    void releaseMsgQue(LList* runlist);

    void calcBytes(EnumDir enDir, GenData* data, 
        unsigned now, unsigned& total, 
        unsigned& total2, unsigned& thresh);
    
private:
    const int m_cap;
    Lock* m_lock; 
    GenData* m_cache;
    GenData** m_datas;
    Queue m_pool;
    unsigned m_conn_timeout;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
};

#endif

