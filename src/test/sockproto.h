#ifndef __SOCKPROTO_H__
#define __SOCKPROTO_H__
#include"isockapi.h"
#include"sockframe.h"


/* the min head size used by protocol user */
static const int MAX_MSG_HEAD_SIZE = 32; 

struct SockBuffer {
    NodeMsg* m_msg; 
    unsigned m_pos;
    char m_head[MAX_MSG_HEAD_SIZE];
};

class GenSockProto {
public:
    GenSockProto(SockFrame* frame);
    ~GenSockProto();

    int parseData(int fd, SockBuffer* cache, 
        const char* buf, int size);
    
    int parseData2(int fd, SockBuffer* cache, 
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
    class GenAccpt : public ISockComm {
    public:
        GenAccpt(SockFrame* frame);
        ~GenAccpt();
        
        virtual void onClose(int hd);
    
        virtual int process(int hd, NodeMsg* msg);

        virtual int parseData(int fd, const char* buf, int size);

    private:
        SockFrame* m_frame;
        GenSockProto* m_proto;
    };
    
public:
    GenSvr(unsigned rd_thresh, 
        unsigned wr_thresh);
    
    virtual ~GenSvr();

    virtual int onNewSock(int parentId, 
        int newId, AccptOption& opt);

    virtual void onClose(int hd);

    static SockBuffer* allocBuffer();
    static void freeBuffer(SockBuffer*);

private:
    SockFrame* m_frame;
    GenAccpt* m_accpt;
    unsigned m_rd_thresh; 
    unsigned m_wr_thresh;
};

class GenCli : public ISockCli {
public:
    GenCli(unsigned rd_thresh, 
        unsigned wr_thresh,
        int pkgSize, int pkgCnt);
    virtual ~GenCli();

    long genExtra();

    virtual int onConnOK(int hd, ConnOption& opt);

    virtual void onConnFail(long, int);

    virtual void onClose(int hd);
    
    virtual int process(int hd, NodeMsg* msg);
    
    virtual int parseData(int fd, 
        const char* buf, int size);

    NodeMsg* genMsg(int size);

private: 
    SockFrame* m_frame;
    GenSockProto* m_proto;
    unsigned m_rd_thresh; 
    unsigned m_wr_thresh;
    int m_pkg_size;
    int m_pkg_cnt;
};


#endif

