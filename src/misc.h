#ifndef __MISC_H__
#define __MISC_H__
#include"shareheader.h"


struct SockParam {
    int m_port;
    int m_addrLen;
    char m_ip[DEF_IP_SIZE];
    char m_addr[MAX_ADDR_SIZE];
}; 

struct ClockTime;

class MiscTool {
public:
    static void getClock(ClockTime* ct);
    static void clock2Time(ClockTime* tm, 
        const ClockTime* clk);
    static long diffMsec(const ClockTime* ct1, 
        const ClockTime* ct2);

    static unsigned long long read8Bytes(int fd);
    static void writeEvent(int fd);
    static void writePipeEvent(int fd);
    static void readPipeEvent(int fd);

    static int creatEvent();
    static int creatTimer(int ms);
    static int creatPipes(int (*)[2]); 
};


class SockTool {
public:
    static int creatListener(const char szIP[], int port, int backlog=10000);
    static int creatConnector(const char szIP[], int port);
    static int acceptSock(int listenFd, int* pfd, SockParam* param);
    static void closeSock(int fd);
    static int setReuse(int fd);

    static int recvTcp(int fd, void* buf, int max);
    static int sendTcp(int fd, const void* buf, int size);

    static bool chkConnectStat(int fd);

    static int chkAddr(const char szIP[], int port);
    static int addr2IP(SockParam* param);
    static int ip2Addr(SockParam* param);

private:    
    static int creatSock();
};


#endif

