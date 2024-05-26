#include<stdarg.h>
#include<unistd.h>
#include<sys/types.h>
#include<regex.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<errno.h>
#include<cstdlib>
#include<cstdio>
#include<cstring>
#include"clog.h"
#include"misc.h"


#define __INIT_RUN__ __attribute__((unused, constructor))
#define __END_RUN__ __attribute__((unused, destructor))

static const char DEF_LOG_LINK_NAME[] = "run.log";
static const int MAX_LOG_NAME_SIZE = 32;
static const char DEF_LOG_NAME_FORMAT[] = "run_%03d.log";
static const char DEF_LOG_NAME_PATTERN[] = "^run_([0-9]{3}).log$";
static const char DEF_NULL_FILE[] = "/dev/null";
static const char DEF_LOG_DIR[] = "logs";
static const int DEF_LOG_FILE_MODE = 0644;

static const char* DEF_LOG_DESC[MAX_LOG_LEVEL] = {
    "ERROR", "WARN", "INFO", "DEBUG", "VERB" 
};

class _Clog { 
public: 
    _Clog();
    ~_Clog();
    
    int init(const char* path); 
    void finish();
    
    int formatLog(int level, const char format[], ...);
    int vformatLog(int level, const char format[], va_list ap);
    
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

_Clog::_Clog() { 
    m_bValid = false;
    
    m_log_fd = 0;
    m_curr_size = 0;
    m_curr_index = 0;
    m_log_level = ENUM_LOG_INFO;

    m_has_stdout = true;
    m_max_size = MAX_LOG_FILE_SIZE;
    m_max_log_cnt = MAX_LOG_FILE_CNT;
    
    MiscTool::bzero(m_dir, sizeof(m_dir));
    setDir(DEF_LOG_DIR);
}

_Clog::~_Clog() {
}

int _Clog::init(const char* dir) {
    int ret = 0;
    int index = 0;

    if (!m_bValid) {
        setDir(dir); 
        
        ret = MiscTool::mkDir(m_dir);
        if (0 == ret) { 
            index = findIndex();

            ret = openLog(index, true); 
        } 

        if (0 == ret) {
            LOG_INFO("prog_start| log_dir=%s| version=%s|"
                " pid=%d| msg=start now|", 
                m_dir, DEF_BUILD_VER, 
                MiscTool::getPid());
        } else {
            fprintf(stderr, "init_log| ret=%d| log_dir=%s|"
                " msg=init log err|\n", ret, m_dir);
        }
    }

    return ret;
}

void _Clog::finish() {
    LOG_INFO("prog_stop| version=%s|"
        " pid=%d| msg=exit now|", 
        DEF_BUILD_VER,
        MiscTool::getPid());
    
    m_bValid = false;
    
    if (0 < m_log_fd) {
        close(m_log_fd);
        m_log_fd = 0;
    }
}

void _Clog::setDir(const char dir[]) {
    int max = sizeof(m_dir);
    int len = 0;
    
    if (NULL != dir && '\0' != dir[0]) {
        len = MiscTool::strLen(dir, max);
        while (0 < len && '/' == dir[len-1]) {
            --len;
        }

        if (0 < len && len < max) {
            MiscTool::strCpy(m_dir, dir, len + 1);
        } 
    }
}

void _Clog::setLogLevel(int level) {
    m_log_level = level;
}

void _Clog::setLogScreen(bool on) {
    m_has_stdout = on;
}

void _Clog::setMaxLogSize(int size) {
    m_max_size = size;
    m_max_size *= 1024 * 1024;
}

void _Clog::setMaxLogCnt(int cnt) {
    if (0 < cnt) {
        m_max_log_cnt = cnt;
    }
}

int _Clog::findIndex() {
    const char* psz = NULL;
    int ret = 0; 
    int index = 0;
    regex_t reg;
    regmatch_t matchs[2]; 
    char name[MAX_LOG_NAME_SIZE] = {0};
    char buf[256] = {0};

    if ('\0' != m_dir[0]) {
        MiscTool::strPrint(buf, sizeof(buf), 
            "%s/%s", m_dir, DEF_LOG_LINK_NAME);
    } else {
        MiscTool::strPrint(buf, sizeof(buf),
            "%s", DEF_LOG_LINK_NAME);
    }
    
    ret = readlink(buf, name, MAX_LOG_NAME_SIZE-1);
    if (0 <= ret) {
        name[ret] = '\0';
        
        ret = regcomp(&reg, DEF_LOG_NAME_PATTERN, REG_EXTENDED);
        if (0 == ret) {
            ret = regexec(&reg, name, 2, matchs, 0);
            if (0 == ret) {
                psz = name + matchs[1].rm_so;
                for (int i=0; i<3; ++i, ++psz) {
                    index = index * 10 + (int)(*psz - '0');
                }
                
                if (0 <= index && m_max_log_cnt > index) {
                    /* ok */
                } else {
                    index = 0;    
                }
            }

            regfree(&reg);
        }
    } 

    return index;
}

int _Clog::openFile(const char name[], bool append) {
    int fd = 0;
    int flag = O_WRONLY | O_CREAT;

    if (!append) {
        flag |= O_TRUNC;
    }
    
    fd = open(name, flag, DEF_LOG_FILE_MODE);
    if (0 < fd) {
        /* ok */
    } else {
        printf("name=%s| err=%s|\n", name, strerror(errno));
        
        /* write to null if harddisk empty */
        fd = open(DEF_NULL_FILE, flag, DEF_LOG_FILE_MODE);
    }

    return fd;
}

int _Clog::openLog(int index, bool append) {
    int ret = 0;
    int fd = 0;
    int size = 0;
    char name[MAX_LOG_NAME_SIZE] = {0};
    char logfile[256] = {0};
    char symfile[256] = {0};

    if (MAX_LOG_FILE_CNT <= index) {
        index = 0;
    }
    
    MiscTool::strPrint(name, MAX_LOG_NAME_SIZE, 
        DEF_LOG_NAME_FORMAT, index);

    if ('\0' != m_dir[0]) {
        MiscTool::strPrint(logfile, sizeof(logfile),
            "%s/%s", m_dir, name);
        MiscTool::strPrint(symfile, sizeof(logfile),
            "%s/%s", m_dir, DEF_LOG_LINK_NAME);
    } else {
        MiscTool::strPrint(logfile, sizeof(logfile),
            "%s", name);
        MiscTool::strPrint(symfile, sizeof(logfile),
            "%s", DEF_LOG_LINK_NAME);
    }
    
    fd = openFile(logfile, append);
    if (0 < fd) {
        size = lseek(fd, 0, SEEK_END);

        m_bValid = true;
        
        if (0 < m_log_fd) {
            ret = dup2(fd, m_log_fd);
            if (-1 != ret) {
                ret = 0;
            }
            
            close(fd);
        } else {
            m_log_fd = fd;
        }

        if (0 == ret) {
            m_curr_size = size;
            m_curr_index = index;
            
            ret = unlink(symfile);
            if (0 == ret || ENOENT == errno) {
                symlink(name, symfile);
            }
        }

        return 0;
    } else {
        m_bValid = false;
        return -1;
    }
}

bool _Clog::isFileMax() const {
    if (m_curr_size < m_max_size) {
        return false;
    } else {
        return true; 
    }
}

int _Clog::preTag(unsigned tid, char stamp[], int max) {
    int cnt = 0;
    ClockTime ct1;
    time_t in;
    struct tm out;

    MiscTool::getTime(&ct1);

    MiscTool::bzero(&out, sizeof(out));
    
    in = ct1.m_sec;
    localtime_r(&in, &out);
    cnt = MiscTool::strPrint(stamp, max, 
        "<%d-%02d-%02d %02d:%02d:%02d.%03u %u> ",
        out.tm_year + 1900, out.tm_mon + 1,
        out.tm_mday, out.tm_hour,
        out.tm_min, out.tm_sec,
        ct1.m_msec,
        tid);
    
    return cnt;
}

int _Clog::formatLog(int level, const char format[], ...) {
    va_list ap;
    int cnt = 0;

    va_start(ap, format);
    cnt = formatLog(level, format, ap);
    va_end(ap);
    
    return cnt;
}

int _Clog::vformatLog(int level, const char format[], va_list ap) { 
    char* psz = NULL;
    int maxlen = 0;
    int size = 0;
    int cnt = 0;
    int index = 0;
    unsigned tid = 0;
    char cache[MAX_LOG_CACHE_SIZE] = {0};

    if (level > m_log_level) {
        return 0;
    }

    index = m_curr_index;
    
    tid = MiscTool::getTid();

    psz = cache;
    maxlen = MAX_LOG_CACHE_SIZE - 1; // last has a LF char

    cnt = MiscTool::strPrint(psz, maxlen,
        "[%s]", DEF_LOG_DESC[level]);
    psz += cnt;
    maxlen -= cnt;
    size += cnt;
    
    cnt = preTag(tid, psz, maxlen);
    psz += cnt;
    maxlen -= cnt;
    size += cnt;
    
    cnt = vsnprintf(psz, maxlen, format, ap);
    if (0 < cnt && cnt < maxlen) {
        psz += cnt;
        maxlen -= cnt;
        size += cnt;
    } else if (cnt >= maxlen) {
        cnt = maxlen;
        psz += cnt;
        maxlen -= cnt;
        size += cnt;
    } else {
        /* nothing */
    }

    *(psz++) = '\n';
    --maxlen;
    ++size;

    if (m_bValid) {
        cnt = write(m_log_fd, cache, size);
        if (0 < cnt) {
            m_curr_size += cnt; 

            if (isFileMax()) {
                openLog(index + 1, false);
            }
        } else {
            cnt = 0;
        }
    } else {
        cnt = 0;
    }
    
    if (m_has_stdout) {
        write(DEF_STDOUT_FD, cache, size); 
    } 

    return cnt;
}


_Clog* Clog::m_log = NULL;

void Clog::initLog() {
    if (NULL == m_log) {
        m_log = new _Clog;
    }
}

void Clog::destroyLog() {
    if (NULL != m_log) { 
        m_log->finish();
        delete m_log;

        m_log = NULL;
    }
}

int Clog::formatLog(int level, const char format[], ...) {
    va_list ap;
    int cnt = 0;

    if (NULL != m_log) { 
        va_start(ap, format);
        cnt = m_log->vformatLog(level, format, ap);
        va_end(ap);
    }
    
    return cnt;
}

int Clog::confLog(const char dir[]) {
   int ret = 0;

   ret = m_log->init(dir);
   return ret;
}

void Clog::setLogLevel(int level) {
    m_log->setLogLevel(level);
}

void Clog::setLogScreen(bool on) {
    m_log->setLogScreen(on);
}

void Clog::setMaxLogSize(int size) {
    m_log->setMaxLogSize(size);
}

void Clog::setMaxLogCnt(int cnt) {
    m_log->setMaxLogCnt(cnt);
}

void __INIT_RUN__ _initLog() {
    Clog::initLog();
}

void __END_RUN__ _finishLog() {
    Clog::destroyLog();
}

