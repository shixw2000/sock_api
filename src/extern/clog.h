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

class Clog {
    static const int MAX_LOG_FILE_SIZE = 0x800000;
    static const int MAX_LOG_FILE_CNT = 3;
    static const int MAX_LOG_NAME_SIZE = 32;
    static const int MAX_LOG_CACHE_SIZE = 0x1000; // 4KB, in stack frame
    static const int DEF_STDOUT_FD = 1;

    Clog();
    ~Clog();

    void finish();

    static void initLog();
    static void destroyLog();

public: 
    static Clog* instance();
    
    int init(const char* path); 
    
    void formatLog(int level, const char format[], ...);
    void setLogLevel(int level);
    void setLogScreen(bool on);

    /* unit: MB */
    void setMaxLogSize(int size);
    
    void setMaxLogCnt(int cnt);
    
private: 
    void setDir(const char dir[]);
    int preTag(unsigned tid, char stamp[], int max);
    int findIndex();
    int openLog(int index, bool append);
    int openFile(const char name[], bool append);
    bool isFileMax() const;

    friend void _initLog();
    friend void _finishLog();
    
private:
    long m_curr_size;
    long m_max_size;
    int m_max_log_cnt;
    int m_curr_index; 
    int m_log_level;
    int m_log_fd;
    bool m_has_stdout;
    bool m_bValid;
    char m_dir[128];
};

#define LOG_VERB(format,args...) Clog::instance()->formatLog(ENUM_LOG_VERB, format, ##args)
#define LOG_DEBUG(format,args...) Clog::instance()->formatLog(ENUM_LOG_DEBUG, format, ##args)
#define LOG_INFO(format,args...) Clog::instance()->formatLog(ENUM_LOG_INFO, format, ##args)
#define LOG_WARN(format,args...) Clog::instance()->formatLog(ENUM_LOG_WARN, format, ##args)
#define LOG_ERROR(format,args...) Clog::instance()->formatLog(ENUM_LOG_ERR, format, ##args)
#define ConfLog Clog::instance()->init
#define SetLogLevel Clog::instance()->setLogLevel
#define SetLogScreen Clog::instance()->setLogScreen
#define SetMaxLogSize Clog::instance()->setMaxLogSize
#define SetMaxLogCnt Clog::instance()->setMaxLogCnt


#endif

