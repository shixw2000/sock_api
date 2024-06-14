#ifndef __SOCKPROTO_H__
#define __SOCKPROTO_H__
#include"isockapi.h"
#include"sockframe.h"
#include"config.h"
#include"socktool.h"


/* the min head size used by protocol user */
static const int MAX_MSG_HEAD_SIZE = 32; 
static const char SERVER_SEC[] = "server";
static const char CLIENT_SEC[] = "client";
static const char UDP_SEC[] = "udp";

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
public:
    GenSvr(Config* conf);
    
    virtual ~GenSvr();

    int init();
    void finish();

    void start();
    void stop();
    void wait();

    virtual int onNewSock(int parentId, 
        int newId, AccptOption& opt);

    virtual void onListenerClose(int hd); 
    virtual void onClose(int hd);

    virtual int process(int hd, NodeMsg* msg);

    virtual int parseData(int fd, const char* buf, int size,
        const SockAddr* addr);

    static SockBuffer* allocBuffer();
    static void freeBuffer(SockBuffer*);

private:
    SockFrame* m_frame;
    GenSockProto* m_proto;
    Config* m_conf;
    unsigned m_rd_thresh; 
    unsigned m_wr_thresh;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    int m_log_level;
    int m_log_stdin;
    int m_log_size;
    AddrInfo m_addr;
};

class GenCli : public ISockCli {
public:
    GenCli(Config* conf);
    virtual ~GenCli();

    int init();
    void finish();

    void start();
    void stop();
    void wait();

    long genExtra();

    virtual int onConnOK(int hd, ConnOption& opt);

    virtual void onConnFail(long, int);

    virtual void onClose(int hd);
    
    virtual int process(int hd, NodeMsg* msg);
    
    virtual int parseData(int fd, 
        const char* buf, int size,
        const SockAddr* addr);

    NodeMsg* genMsg(int size);

private: 
    SockFrame* m_frame;
    GenSockProto* m_proto;
    Config* m_conf;
    unsigned m_rd_thresh; 
    unsigned m_wr_thresh;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    int m_pkg_size;
    int m_first_send_cnt;
    int m_cli_cnt;
    int m_log_level;
    int m_log_stdin;
    int m_log_size;
    AddrInfo m_addr;
};

class GenUdp : public ISockBase { 
public:
    GenUdp(Config* conf);
    
    virtual ~GenUdp();

    int init();
    void finish();

    void start();
    void stop();
    void wait();

    virtual void onTimerPerSec();

    virtual void onClose(int hd);

    virtual int process(int hd, NodeMsg* msg);

    virtual int parseData(int fd, const char* buf, int size,
        const SockAddr* addr);

    NodeMsg* genUdpMsg(int size, const SockAddr& addr);

private:
    SockFrame* m_frame;
    TimerObj* m_timer_obj;
    Config* m_conf;
    int m_udp_fd;
    unsigned m_rd_thresh; 
    unsigned m_wr_thresh;
    unsigned m_rd_timeout;
    unsigned m_wr_timeout;
    int m_pkg_size;
    int m_log_level;
    int m_log_stdin;
    int m_log_size;
    int m_is_recv;
    bool m_is_multicast;
    AddrInfo m_uni_info;
    AddrInfo m_multi_info;
    AddrInfo m_peer_info;
    SockAddr m_peer_addr;
};

#endif

