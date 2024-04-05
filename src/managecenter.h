#ifndef __MANAGECENTER_H__
#define __MANAGECENTER_H__
#include"sockdata.h"


class Lock;
class SockProto;

class ManageCenter {
    static const int MAX_BUFF_SIZE = 1024 * 1024;
    static const int MAX_FD_NUM = 600;
    
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

    void setData(GenData* data, long extra = 0, long data2 = 0);

    int getStat(EnumDir enDir, GenData* data) const;
    void setStat(EnumDir enDir, GenData* data, int stat);
    void setCb(EnumDir enDir, GenData* data, int cb);
    
    unsigned getRdThresh(GenData* data) const;
    unsigned getWrThresh(GenData* data) const;
    
    /* unit: bytes */
    void setSpeed(GenData* data, unsigned rd_thresh, unsigned wr_thresh);

    /* ticks */
    void setTimeout(unsigned rd_timeout, unsigned wr_timeout);
    void setAddr(GenData* data, const char szIP[], int port);

    bool setClose(GenData* data);

    void lock();
    void unlock();
    
    GenData* find(int fd) const;
    bool exists(int fd) const; 

    GenData* reg(int fd);
    void unreg(int fd); 

    NodeCmd* creatCmdComm(EnumSockCmd cmd, int fd); 
    NodeCmd* creatCmdSchedTask(unsigned delay,
        TFunc func, long data, long data2);

    void setProto(SockProto* proto);
    int writeMsg(int fd, NodeMsg* pMsg, int max);
    int recvMsg(int fd, SockBuffer* buffer, int max);
    
private:
    GenData* _allocData();
    void _releaseData(GenData* ele);

    void calcBytes(EnumDir enDir, GenData* data, 
        unsigned now, unsigned& total, 
        unsigned& total2, unsigned& thresh);
    
private:
    const int m_cap;
    Lock* m_lock; 
    GenData* m_cache;
    GenData** m_datas;
    SockProto* m_proto;
    Queue m_pool;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    char m_buf[MAX_BUFF_SIZE];
};

#endif

