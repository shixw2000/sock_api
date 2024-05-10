#ifndef __SOCKTOOL_H__
#define __SOCKTOOL_H__


static const int DEF_IP_SIZE = 64;
static const int MAX_ADDR_SIZE = 256; 

struct iovec;

struct SockParam {
    int m_port;
    int m_addrLen;
    char m_ip[DEF_IP_SIZE];
    char m_addr[MAX_ADDR_SIZE];
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

    static int sendVec(int fd, struct iovec* iov,
        int size, int maxlen);
    
    static int recvVec(int fd, struct iovec* iov, 
        int size, int maxlen);

    static int sendBuffers(int fd,
        const void* buf1, int size1,
        const void* buf2, int size2);

    static bool chkConnectStat(int fd);

    static int chkAddr(const char szIP[], int port);
    static int addr2IP(SockParam* param);
    static int ip2Addr(SockParam* param);

private:    
    static int creatSock();
};

#endif

