#ifndef __ISOCKAPI_H__
#define __ISOCKAPI_H__
#include"shareheader.h" 


struct NodeMsg;

/* the min head size used by protocol user */
static const int MAX_MSG_HEAD_SIZE = 32; 

struct SockBuffer {
    NodeMsg* m_msg; 
    unsigned m_pos;
    char m_head[MAX_MSG_HEAD_SIZE];
};

class SockProto {
public: 
    virtual ~SockProto() {}

    virtual bool parseData(int fd, SockBuffer* cache, 
        const char* buf, int size) = 0;
};

class ISockBase {
public:
    virtual ~ISockBase() {}
    
    virtual void onClose(long data2, int hd) = 0;
    
    virtual int process(long data2, int hd, NodeMsg* msg) = 0;
};

class ISockSvr : public ISockBase {
public:
    virtual ~ISockSvr() {} 
    virtual int onNewSock(long data2, int parentId, int newId) = 0;
};

class ISockCli : public ISockBase {
public:
    virtual ~ISockCli() {}
    virtual int onConnOK(long data2, int hd) = 0;
    virtual void onConnFail(long data2, int hd) = 0;
};

#endif

