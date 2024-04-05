#ifndef __SOCKPROTO_H__
#define __SOCKPROTO_H__
#include"isockapi.h"
#include"sockframe.h"


class GenSockProto : public SockProto {
public:
    GenSockProto();
    ~GenSockProto();

    virtual bool parseData(int fd, SockBuffer* cache, 
        const char* buf, int size);
    
    bool parseData2(int fd, SockBuffer* cache, 
        const char* buf, int size); 

private: 
    int dispatch(int fd, NodeMsg* pMsg);
    
    int parseBody(int fd, SockBuffer* buffer, 
        const char* input, int len);

    int parseHead(SockBuffer* buffer,
        const char* input, int len);

    bool analyseHead(SockBuffer* buffer);

private:
    SockFrame* m_frame;
};


class GenSvr : public ISockSvr {
public:
    GenSvr();
    virtual ~GenSvr() {}

    virtual int onNewSock(long data2, int parentId, int newId);

    virtual void onClose(long data2, int hd);
    
    virtual int process(long data2, int hd, NodeMsg* msg);

private:
    SockFrame* m_frame;
};

class GenCli : public ISockCli {
public:
    GenCli(int pkgSize, int pkgCnt);
    virtual ~GenCli() {}

    virtual int onConnOK(long data2, int hd);

    virtual void onConnFail(long data2, int hd);

    virtual void onClose(long data2, int hd);
    
    virtual int process(long data2, int hd, NodeMsg* msg);

    NodeMsg* genMsg(int size);

private:
    int m_pkg_size;
    int m_pkg_cnt;
    SockFrame* m_frame;
};


#endif

