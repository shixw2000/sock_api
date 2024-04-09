#ifndef __MANAGECENTER_H__
#define __MANAGECENTER_H__
#include"sockdata.h"


class Lock;
class SockProto;
class TickTimer;

class ManageCenter {
    static const int MAX_BUFF_SIZE = 1024 * 1024;
    static const int MAX_FD_NUM = 600;
    static const unsigned KILO = 1024;
    
public:
    ManageCenter();
    ~ManageCenter();

    int init();
    void finish(); 

    int capacity() const {
        return m_cap;
    }

    bool chkExpire(EnumDir enDir, const GenData* data, unsigned now); 
    bool updateExpire(EnumDir enDir, GenData* data, unsigned now);

    void recordBytes(EnumDir enDir, GenData* data, 
        unsigned now, unsigned total);
    
    void clearBytes(EnumDir enDir, 
        GenData* data, unsigned now); 
    
    unsigned calcThresh(EnumDir enDir, 
        GenData* data, unsigned now);

    int getFd(GenData* ele) const;

    int getRdIndex(GenData* ele) const;
    void setRdIndex(GenData* ele, int index);

    void setData(GenData* data, long extra = 0, long data2 = 0);

    int getStat(EnumDir enDir, GenData* data) const;
    void setStat(EnumDir enDir, GenData* data, int stat);
    void setCb(EnumDir enDir, GenData* data, int cb);
    int getCb(EnumDir enDir, GenData* data) const;

    void setConnRes(GenData* data, int res);

    unsigned getRdThresh(GenData* data) const;
    unsigned getWrThresh(GenData* data) const;
    
    /* unit: kilo bytes */
    void setSpeed(GenData* data, unsigned rd_thresh, unsigned wr_thresh);
    
    /* seconds */
    void setTimeout(unsigned rd_timeout, unsigned wr_timeout);
    
    void setIdleTimeout(GenData* data);
    void setConnTimeout(GenData* data);

    void refData(GenData* dstData, GenData* srcData); 
    
    void setAddr(GenData* data, const char szIP[], int port);

    void lock();
    void unlock();
    
    GenData* find(int fd) const;
    bool exists(int fd) const; 
    
    bool isClosed(GenData* data) const;
    bool markClosed(GenData* data);

    GenData* reg(int fd);
    void unreg(int fd); 

    NodeCmd* creatCmdComm(EnumSockCmd cmd, int fd); 
    NodeCmd* creatCmdSchedTask(unsigned delay,
        TFunc func, long data, long data2);

    void setProto(SockProto* proto);
    int writeMsg(int fd, NodeMsg* pMsg, int max);
    int recvMsg(GenData* data, int max);
    int addMsg(EnumDir enDir, GenData* data, NodeMsg* pMsg);
    void appendQue(EnumDir enDir, LList* to, GenData* data);
    LList* getWrPrivQue(GenData* data) const;

    void addNode(EnumDir enDir, LList* root, GenData* data);
    void delNode(EnumDir enDir, GenData* data);
    GenData* fromNode(EnumDir enDir, LList* node);
    
    void addTimeout(EnumDir enDir, LList* root, GenData* data);
    void delTimeout(EnumDir enDir, GenData* data);
    GenData* fromTimeout(EnumDir enDir, LList* node);
    
    void flowctl(EnumDir enDir, TickTimer* timer, GenData* data);
    void unflowctl(EnumDir enDir, GenData* data);
    void setFlowctl(EnumDir enDir, GenData* data, TFunc func, long ptr);

    int onNewSock(GenData* data, int newFd);
    int onConnect(GenData* data);
    int onProcess(GenData* data, NodeMsg* pMsg);
    void onClose(GenData* data);
    
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
    Lock* m_lock; 
    GenData* m_cache;
    GenData** m_datas;
    SockProto* m_proto;
    Queue m_pool;
    unsigned m_conn_timeout;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    char m_buf[MAX_BUFF_SIZE];
};

#endif

