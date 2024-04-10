#ifndef __SOCKDATA_H__
#define __SOCKDATA_H__
#include"llist.h"
#include"isockapi.h"
#include"shareheader.h"


struct NodeMsg;
struct NodeCmd;
struct GenData; 

static const int DEF_TICK_MSEC = 250;
static const unsigned DEF_NUM_PER_SEC = 4;
static const int MASK_PER_SEC = 0x3;
static const int DEF_FLOWCTL_TICK_NUM = 2;
static const int DEF_POLL_TIME_MSEC = 1000;

enum EnumRdCb {
    ENUM_RD_DEFAULT = 0,
    ENUM_RD_SOCK,
    ENUM_RD_LISTENER,

    ENUM_RD_END
};

enum EnumWrCb {
    ENUM_WR_DEFAULT = 0,
    ENUM_WR_SOCK,
    ENUM_WR_Connector,

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
    ENUM_STAT_INIT,
    ENUM_STAT_IDLE,
    ENUM_STAT_BLOCKING,
    ENUM_STAT_READY,
    ENUM_STAT_FLOWCTL,
    ENUM_STAT_DISABLE,
    ENUM_STAT_TIMEOUT,
    ENUM_STAT_CLOSING,
    
    ENUM_STAT_ERROR 
};

enum EnumDir {
    ENUM_DIR_RECVER,
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
    ENUM_CMD_CLOSE_FD,

    ENUM_CMD_SCHED_TASK,

    ENUM_CMD_END
};

struct TimerObj;
typedef void (*TFunc)(long, long);

struct TimerObj {
    HList m_node; 
    TFunc func;
    long m_data;
    long m_data2;
    unsigned m_expire;
    unsigned m_interval;
    bool m_bDel;
};

struct CmdHead_t {
    int m_cmd;
    char m_body[0];
};

struct CmdComm {
    int m_fd;
};

struct CmdSchedTask {
    TFunc func;
    long m_data;
    long m_data2;
    unsigned m_delay; 
};

struct CmdSetFLowCtrl {
    int m_dir;
    int m_kps;
};

struct CmdSetTimeout {
    int m_dir;
    int m_timeout;
}; 


#endif

