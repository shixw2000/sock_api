#include<cstdlib>
#include<cstdio>
#include"sockproto.h"
#include"misc.h"
#include"config.h"


int testSrv(Config* conf) {
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

    delete psvr;

    LOG_INFO("ret=%d| end test svr|", ret);
    
    return ret;
}

int testCli(Config* conf) {
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

    delete pcli;

    LOG_INFO("ret=%d| end test cli|", ret);
    
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
    } else {
        ret = testSrv(conf);
    }

    return ret;
}

