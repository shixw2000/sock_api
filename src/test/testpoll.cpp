#include<cstdlib>
#include"sockframe.h"
#include"sockproto.h"


int testSrv(const char ip[], int port,
    unsigned rd_thresh, unsigned wr_thresh,
    unsigned rd_timeout, unsigned wr_timeout) {
    int ret = 0;
    SockFrame* frame = NULL;
    SockProto* proto = NULL;
    ISockSvr* psvr = NULL;

    LOG_INFO("test poll|");

    frame = SockFrame::instance();
    proto = new GenSockProto;
    psvr = new GenSvr;

    do { 
        frame->setProto(proto);
        frame->setTimeout(rd_timeout, wr_timeout);
        frame->creatSvr(ip, port, psvr, 
            0, rd_thresh, wr_thresh);

        frame->start();
        frame->wait();
    } while (0);

    SockFrame::destroy(frame);
    delete proto;

    LOG_INFO("ret=%d| end test svr|", ret);
    
    return ret;
}

int testCli(const char ip[], int port,
    int cliCnt, int pkgSize, int pkgCnt) {
    int ret = 0;
    SockFrame* frame = NULL;
    SockProto* proto = NULL;
    ISockCli* pcli = NULL;

    LOG_INFO("test poll|");

    frame = SockFrame::instance();
    proto = new GenSockProto;
    pcli = new GenCli(pkgSize, pkgCnt);

    do { 
        frame->setProto(proto);
        frame->setTimeout(60, 60);

        for (int i=0; i<cliCnt; ++i) {
            frame->sheduleCli(1, ip, port, pcli, 0);
        }
        
        frame->start();
        frame->wait();
    } while (0);

    SockFrame::destroy(frame);
    delete proto;

    LOG_INFO("ret=%d| end test cli|", ret);
    
    return ret;
}

void usage(const char* prog) {
    LOG_ERROR("usage: %s <isSvr> <ip> <port>, examples:\n"
        "server: %s 1 <ip> <port> [rd_thresh] [wr_thresh], or\n"
        "client: %s 0 <ip> <port> <cli_cnt> <pkg_len> <init_pkg_cnt>\n", 
        prog, prog, prog);
}

int testPoll(int argc, char* argv[]) {
    const char* ip = NULL;
    int port = 0;
    int opt = 0;
    int cliCnt = 0;
    int pkgSize = 0;
    int pkgCnt = 0;
    unsigned rd_thresh = 0;
    unsigned wr_thresh = 0;
    unsigned rd_timeout = 0;
    unsigned wr_timeout = 0;

    if (2 <= argc) {
        opt = atoi(argv[1]);
    }

    if (1 == opt) { 
        if (4 <= argc) {
            ip = argv[2];
            port = atoi(argv[3]);

            if (6 <= argc) {
                rd_thresh = atoi(argv[4]);
                wr_thresh = atoi(argv[5]); 

                if (8 <= argc) {
                    rd_timeout = atoi(argv[6]);
                    wr_timeout = atoi(argv[7]);
                }
            }
            
            testSrv(ip, port, rd_thresh, wr_thresh,
                rd_timeout, wr_timeout);
        } else {
            usage(argv[0]);
        }
    } else if (0 == opt) { 
        if (7 == argc) {
            ip = argv[2];
            port = atoi(argv[3]);
            cliCnt = atoi(argv[4]);
            pkgSize = atoi(argv[5]);
            pkgCnt = atoi(argv[6]);
            
            testCli(ip, port, cliCnt, pkgSize, pkgCnt);
        } else {
            usage(argv[0]);
        }
    } else {
        usage(argv[0]);
    }
    
    return 0;
}
