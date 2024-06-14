#ifndef __SOCKDATA_H__
#define __SOCKDATA_H__
#include"llist.h"
#include"isockapi.h"
#include"shareheader.h"


struct NodeMsg;
struct GenData; 

static const int DEF_TICK_MSEC = 250;
static const unsigned DEF_NUM_PER_SEC = 1000 / DEF_TICK_MSEC;
static const int MASK_PER_SEC = 0x3;
static const int DEF_FLOWCTL_TICK_NUM = 2;
static const int DEF_POLL_TIME_MSEC = 1000;

static const int MAX_IOVEC_SIZE = 100;
static const int MAX_SIZE_ONCE_RDWR = 1024 * 1024 * 10;
static const int MAX_BUFF_SIZE = 1024 * 1024;
static const int DEF_CONN_TIMEOUT_TICK = 3 * DEF_NUM_PER_SEC;
static const int DEF_RDWR_TIMEOUT_TICK = 60 * DEF_NUM_PER_SEC;
static const int DEF_LISTENER_SLEEP_TICK = 10 * DEF_NUM_PER_SEC;

enum EnumRdCb {
    ENUM_RD_DEFAULT = 0,
    ENUM_RD_SOCK,
    ENUM_RD_LISTENER,
    ENUM_RD_UDP,

    ENUM_RD_END
};

enum EnumWrCb {
    ENUM_WR_DEFAULT = 0,
    ENUM_WR_SOCK,
    ENUM_WR_Connector,
    ENUM_WR_UDP,

    ENUM_WR_END
};

enum EnumDealCb {
    ENUM_DEAL_DEFAULT = 0,
    ENUM_DEAL_SOCK,
    ENUM_DEAL_Connector,
    ENUM_DEAL_LISTENER,

    ENUM_DEAL_END
};

enum EnumSockStat {
    ENUM_STAT_INVALID = 0,
    ENUM_STAT_IDLE,
    ENUM_STAT_BLOCKING,
    ENUM_STAT_READY,
    ENUM_STAT_FLOWCTL,
    ENUM_STAT_DISABLE,
    
    ENUM_STAT_END 
};

enum EnumDir {
    ENUM_DIR_RECVER = 0,
    ENUM_DIR_SENDER,
    ENUM_DIR_DEALER,
    
    ENUM_DIR_END
};

enum EnumSockCmd {    
    ENUM_CMD_SET_FLOWCTL,
    ENUM_CMD_SET_TIMEOUT, 

    ENUM_CMD_ADD_FD,
    ENUM_CMD_REMOVE_FD,
    ENUM_CMD_DISABLE_FD,
    ENUM_CMD_DELAY_FD,
    ENUM_CMD_UNDELAY_FD,

    ENUM_CMD_END
};

struct TimerObj {
    HList m_node; 
    TimerFunc func;
    long m_data;
    long m_data2;
    unsigned m_expire;
    unsigned m_interval;
};

struct CmdComm {
    int m_fd;
};

#endif

