#ifndef __SOCKFRAME_H__
#define __SOCKFRAME_H__
#include"isockapi.h"


typedef void (*TFunc)(long, long);
class SockProto;

class SockFrame {
private:
    struct _intern;
    
    SockFrame();
    ~SockFrame();

    int init();
    void finish();

public:
    static SockFrame* instance();
    static void destroy(SockFrame*); 

    /* msg functions */
    static NodeMsg* creatNodeMsg(int size);
    static void freeNodeMsg(NodeMsg* pb);

    static bool completedMsg(NodeMsg* pb);
    static char* getMsg(const NodeMsg* pb); 
    static NodeMsg* refNodeMsg(NodeMsg* pb);

    static int getMsgSize(const NodeMsg* pb); 
    static int getMsgPos(const NodeMsg* pb);
    static void setMsgPos(NodeMsg* pb, int pos);
    static void skipMsgPos(NodeMsg* pb, int pos);

    void start();
    void stop();
    void wait();

    void setProto(SockProto* proto);
    void setTimeout(unsigned rd_timeout, 
        unsigned wr_timeout);
    
    int sendMsg(int fd, NodeMsg* pMsg);
    int dispatch(int fd, NodeMsg* pMsg);
    void closeData(int fd);

    int creatSvr(const char szIP[], int port, 
        ISockSvr* svr, long data2, 
        unsigned rd_thresh = 0, unsigned wr_thresh = 0);

    void sheduleCli(unsigned delay, const char szIP[], int port,
        ISockCli* base, long data2, 
        unsigned rd_thresh = 0, unsigned wr_thresh = 0); 

    int creatCli(const char szIP[], int port,
        ISockCli* base, long data2,
        unsigned rd_thresh = 0, unsigned wr_thresh = 0);

    int schedule(unsigned delay, TFunc func, long data, long data2);

private:
    _intern* m_intern;
};

#endif

