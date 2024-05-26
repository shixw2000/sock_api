#include<cstdlib>
#include<cstdio>
#include"sockproto.h"
#include"misc.h"
#include"config.h"


static int testSrv(Config* conf) {
    int ret = 0;
    GenSvr* psvr = NULL;

    LOG_INFO("test poll|");

    psvr = new GenSvr(conf);

    do { 
        ret = psvr->init();
        if (0 != ret) {
            break;
        }
        
        psvr->start();
        psvr->wait();
    } while (0);

    psvr->finish();
    delete psvr;

    LOG_INFO("ret=%d| end test svr|", ret);
    
    return ret;
}

static int testCli(Config* conf) {
    int ret = 0;
    GenCli* pcli = NULL;

    LOG_INFO("test poll|");

    pcli = new GenCli(conf);

    do {
        ret = pcli->init();
        if (0 != ret) {
            break;
        }
        
        pcli->start();
        pcli->wait();
    } while (0);

    pcli->finish();
    delete pcli;

    LOG_INFO("ret=%d| end test cli|", ret);
    
    return ret;
}

static int testUdp(Config* conf) {
    int ret = 0;
    GenUdp* udp = NULL;

    LOG_INFO("test poll|");

    udp = new GenUdp(conf);

    do {
        ret = udp->init();
        if (0 != ret) {
            break;
        }
        
        udp->start();
        udp->wait();
    } while (0);

    udp->finish();
    delete udp;

    LOG_INFO("ret=%d| end test udp|", ret);
    
    return ret;
}

void usage(const char* prog) {
    fprintf(stderr, "usage: %s [conf_file],\n"
        "example: %s service.conf\n", 
        prog, prog);
}

int testPoll(int argc, char* argv[]) {
    int opt = 0;
    int ret = 0;
    const char* path = NULL;
    Config* conf = NULL;

    conf = new Config;

    if (1 == argc) {
        path = NULL;
    } else if (2 == argc) {
        path = argv[1];
    } else {
        usage(argv[0]);
        return -1;
    }

    armSigs();
    
    ret = conf->parseFile(path);
    if (0 != ret) {
        return ret;
    }

    ret = conf->getNum(GLOBAL_SEC, "role", opt);
    if (0 != ret) return ret;

    if (0 == opt) {
        ret = testCli(conf);
    } else if (1 == opt) {
        ret = testSrv(conf);
    } else if (2 == opt) {
        ret = testUdp(conf);
    }

    delete conf;
    return ret;
}

