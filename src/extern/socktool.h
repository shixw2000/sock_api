#ifndef __SOCKTOOL_H__
#define __SOCKTOOL_H__
#include"shareheader.h"


class SockTool {
public:
    static int openAddr(int* pfd, 
        const SockAddr& addr, int backlog);
    
    static int openName(int* pfd,
        const SockName& name, int backlog); 
    
    static int connAddr(int* pfd, const SockAddr& addr); 
    static int connName(int* pfd, const SockName& name);

    static int openUdpAddr(int* pfd, const SockAddr& addr); 
    static int openUdpName(int* pfd, const SockName& name);

    static int openMultiUdp(int* pfd, int port,
        const char my_ip[], const char multi_ip[]);
    
    static EnumSockRet acceptSock(int listenFd, 
        int* pfd, SockAddr* addr);
    
    static void closeSock(int fd);
    static int setReuse(int fd);

    static EnumSockRet recvTcp(int fd, 
        void* buf, int max, int* prdLen);

    static EnumSockRet sendTcp(int fd, 
        const void* buf, int size, int* pwrLen); 

    static EnumSockRet sendTcpUntil(int fd, 
        const void* buf, int size, int* pwrLen);

    static EnumSockRet sendVec(int fd, 
        struct iovec* iov, int size,
        int* pwrLen, const SockAddr* dst = NULL);

    static EnumSockRet sendVecUntil(int fd, 
        struct iovec* iov, int size, 
        int* pwrLen, const SockAddr* dst = NULL);
    
    static EnumSockRet recvVec(int fd,
        struct iovec* iov, int size, 
        int* prdLen, SockAddr* src = NULL);

    static bool chkConnectStat(int fd);

    static int chkAddr(const char szIP[], int port);
    static int addr2IP(SockName* name, const SockAddr* addr);
    static int ip2Addr(SockAddr* addr, const SockName* name);

    static void setNonblock(int fd);

    static int creatSock(int family, int type, int proto);
    static int creatTcp(int* pfd);
    static int creatUdp(int* pfd);
    static int listenFd(int fd, int backlog);
    static int bindName(int fd, const SockName& name);
    static int bindAddr(int fd, const SockAddr& addr);
    
    static void assign(SockName& name,
        const char szIP[], int port);

    static void setName(SockName& dst, const SockName& src);
    static void setAddr(SockAddr& dst, const SockAddr& src);

    static int getLocalSock(int fd, SockAddr& addr);
    static int getPeerSock(int fd, SockAddr& addr);

    static int setLinger(int fd);
    static int setSockBuf(int fd, int KB, bool isRcv);
    static int getSockBuf(int fd, int* pKB, bool isRcv);

    static int setMultiTtl(int fd, int ttl);
    
    static int addMultiMem(int fd, 
        const char my_ip[], const char multi_ip[]);

    static int dropMultiMem(int fd, 
        const char my_ip[], const char multi_ip[]);

    static int blockSrcInMulti(int fd, 
        const char my_ip[], const char multi_ip[], 
        const char src_ip[]);

    static void closeMultiSock(int fd,
        const char my_ip[], const char multi_ip[]);
    
    static int ip2Net(void* buf, const char ip[]);
    static int net2IP(char ip[], int max, const void* net);
};


#endif

