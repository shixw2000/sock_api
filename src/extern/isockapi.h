#ifndef __ISOCKAPI_H__
#define __ISOCKAPI_H__
#include"shareheader.h" 


struct NodeMsg; 

class ISockBase {
public:
    virtual ~ISockBase() {}
    
    virtual void onClose(int hd) = 0; 
};

class ISockComm : public ISockBase {
public:
    virtual int parseData(int fd, 
        const char* buf, int size) = 0;
    
    virtual int process(int hd, NodeMsg* msg) = 0;
};

struct AccptOption {
    ISockComm* m_sock;
    long m_extra;
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    bool m_delay;
};

struct ConnOption {
    unsigned m_rd_thresh;
    unsigned m_wr_thresh;
    bool m_delay;
};

class ISockSvr : public ISockBase {
public:
    virtual int onNewSock(int parentId,
        int newId, AccptOption& opt) = 0;
}; 

class ISockCli : public ISockComm {
public:
    virtual int onConnOK(int hd, ConnOption& opt) = 0;
    virtual void onConnFail(long extra, int errcode) = 0;
};

#endif

