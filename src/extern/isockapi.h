#ifndef __ISOCKAPI_H__
#define __ISOCKAPI_H__
#include"shareheader.h" 


class ITimerCb {
public:
    virtual ~ITimerCb() {}

    virtual void onTimerPerSec() = 0;
};

class ISockBase {
public:
    virtual ~ISockBase() {}

    virtual int parseData(int fd, 
        const char* buf, int size, 
        const SockAddr* addr) = 0;
    
    virtual int process(int hd, NodeMsg* msg) = 0;
    
    virtual void onClose(int hd) = 0; 
}; 

class ISockSvr : public ISockBase {
public:
    virtual int onNewSock(int parentId,
        int newId, AccptOption& opt) = 0;

    virtual void onListenerClose(int hd) = 0; 
}; 

class ISockCli : public ISockBase {
public:
    virtual int onConnOK(int hd, ConnOption& opt) = 0;
    virtual void onConnFail(long extra, int errcode) = 0;
};

#endif

