#ifndef __CLOG_H__
#define __CLOG_H__


enum EnumLogLevel {
    ENUM_LOG_ERR = 0,
    ENUM_LOG_WARN,
    ENUM_LOG_INFO,
    ENUM_LOG_DEBUG,
    ENUM_LOG_VERB,
    
    MAX_LOG_LEVEL
};

static const int MAX_LOG_FILE_SIZE = 0x800000;
static const int MAX_LOG_FILE_CNT = 3; 
static const int MAX_LOG_CACHE_SIZE = 0x1000; // 4KB, in stack frame
static const int DEF_STDOUT_FD = 1;

class _Clog;

class Clog {
private:
    static void initLog();
    static void destroyLog();

    friend void _initLog(); 
    friend void _finishLog();

public:   
    static int formatLog(int level, const char format[], ...);

    static int confLog(const char dir[]);
    
    static void setLogLevel(int level);
    static void setLogScreen(bool on);

    /* unit: MB */
    static void setMaxLogSize(int size);
    
    static void setMaxLogCnt(int cnt);

private:
    static _Clog* m_log;
};

#define LOG_VERB(format,args...) Clog::formatLog(ENUM_LOG_VERB, format, ##args)
#define LOG_DEBUG(format,args...) Clog::formatLog(ENUM_LOG_DEBUG, format, ##args)
#define LOG_INFO(format,args...) Clog::formatLog(ENUM_LOG_INFO, format, ##args)
#define LOG_WARN(format,args...) Clog::formatLog(ENUM_LOG_WARN, format, ##args)
#define LOG_ERROR(format,args...) Clog::formatLog(ENUM_LOG_ERR, format, ##args)
#define ConfLog(x) Clog::confLog(x)
#define SetLogLevel(x) Clog::setLogLevel(x)
#define SetLogScreen(x) Clog::setLogScreen(x)
#define SetMaxLogSize(x) Clog::setMaxLogSize(x)
#define SetMaxLogCnt(x) Clog::setMaxLogCnt(x)


#endif

