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
static const char DEF_LOG_NAME_FORMAT[] = "run_%03d.log";
static const char DEF_LOG_NAME_PATTERN[] = "^run_([0-9]{3}).log$";
static const char DEF_NULL_FILE[] = "/dev/null";
static const char DEF_LOG_DIR[] = "logs";
static const int DEF_LOG_FILE_MODE = 0644;

static const char* DEF_LOG_DESC[MAX_LOG_LEVEL] = {
    "ERROR", "WARN", "INFO", "DEBUG", "VERB" 
};

static Clog* g_log = NULL;

Clog* Clog::instance() {
    return g_log;
}

void Clog::initLog() {
    if (NULL == g_log) {
        g_log = new Clog;

        g_log->allocCache();
    }
}

void Clog::destroyLog() {
    if (NULL != g_log) { 
        g_log->finish();
        delete g_log;

        g_log = NULL;
    }
}

void __INIT_RUN__ _initLog() {
    Clog::initLog();
}

void __END_RUN__ _finishLog() {
    Clog::destroyLog();
}

Clog::Clog() { 
    m_bValid = false;
    
    m_log_fd = 0;
    m_curr_size = 0;
    m_curr_index = 0;
    m_log_level = ENUM_LOG_INFO;

    m_has_stdout = true;
    m_max_size = MAX_LOG_FILE_SIZE;
    m_max_log_cnt = MAX_LOG_FILE_CNT;

    memset(m_caches, 0, sizeof(m_caches)); 
    
    memset(m_dir, 0, sizeof(m_dir));
    setDir(DEF_LOG_DIR);
}

Clog::~Clog() {
}

int Clog::allocCache() {
    char* psz = NULL;
    int max = 0;
    
    max = MAX_CACHE_INDEX * MAX_LOG_CACHE_SIZE;
    psz = (char*)malloc(max);
    if (NULL != psz) {
        for (int i=0; i<MAX_CACHE_INDEX; ++i) {
            m_caches[i] = psz;
            psz += MAX_LOG_CACHE_SIZE;
        }

        return 0;
    } else {
        return -1;
    }
}

int Clog::init(const char* dir) {
    int ret = 0;
    int index = 0;

    setDir(dir);

    if (NULL == m_caches[0]) {
        ret = allocCache();
        if (0 != ret) {
            return -1;
        }
    } 

    ret = MiscTool::mkDir(m_dir);
    if (0 == ret) { 
        index = findIndex();

        ret = openLog(index, true); 
    } 

    if (0 == ret) {
        LOG_INFO("prog_start| log_dir=%s| version=%s|"
            " msg=start now|", m_dir, DEF_BUILD_VER);
    } else {
        fprintf(stderr, "init_log| ret=%d| log_dir=%s|"
            " msg=init log err|\n", ret, m_dir);
    }

    return ret;
}

void Clog::finish() {
    LOG_INFO("prog_stop| version=%s| msg=exit now|", DEF_BUILD_VER);
    
    m_bValid = false;
    
    if (0 < m_log_fd) {
        close(m_log_fd);
        m_log_fd = 0;
    }

    if (NULL != m_caches[0]) {
        free(m_caches[0]);
        memset(m_caches, 0, sizeof(m_caches));
    }
}

void Clog::setDir(const char dir[]) {
    int max = sizeof(m_dir) - 1;
    int len = 0;
    
    if (NULL != dir && '\0' != dir[0]) {
        len = strnlen(dir, max);
        while (0 < len && '/' == dir[len-1]) {
            --len;
        }

        if (0 < len) {
            strncpy(m_dir, dir, len);
            m_dir[len] = '\0';
        } 
    }
}

void Clog::setLogLevel(int level) {
    m_log_level = level;
}

void Clog::setLogScreen(bool on) {
    m_has_stdout = on;
}

void Clog::setMaxLogSize(int size) {
    m_max_size = size;
    m_max_size *= 1024 * 1024;
}

void Clog::setMaxLogCnt(int cnt) {
    if (0 < cnt) {
        m_max_log_cnt = cnt;
    }
}

int Clog::findIndex() {
    const char* psz = NULL;
    int ret = 0; 
    int index = 0;
    regex_t reg;
    regmatch_t matchs[2]; 
    char name[MAX_LOG_NAME_SIZE] = {0};
    char buf[256] = {0};

    if ('\0' != m_dir[0]) {
        snprintf(buf, sizeof(buf), "%s/%s", m_dir, DEF_LOG_LINK_NAME);
    } else {
        snprintf(buf, sizeof(buf), "%s", DEF_LOG_LINK_NAME);
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

int Clog::openFile(const char name[], bool append) {
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

int Clog::openLog(int index, bool append) {
    int ret = 0;
    int fd = 0;
    int size = 0;
    char name[MAX_LOG_NAME_SIZE] = {0};
    char logfile[256] = {0};
    char symfile[256] = {0};

    if (MAX_LOG_FILE_CNT <= index) {
        index = 0;
    }
    
    snprintf(name, MAX_LOG_NAME_SIZE, 
        DEF_LOG_NAME_FORMAT, index);

    if ('\0' != m_dir[0]) {
        snprintf(logfile, sizeof(logfile),
            "%s/%s", m_dir, name);
        snprintf(symfile, sizeof(logfile),
            "%s/%s", m_dir, DEF_LOG_LINK_NAME);
    } else {
        snprintf(logfile, sizeof(logfile),
            "%s", name);
        snprintf(symfile, sizeof(logfile),
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

bool Clog::isFileMax() const {
    if (m_curr_size < m_max_size) {
        return false;
    } else {
        return true; 
    }
}

int Clog::preTag(unsigned tid, char stamp[], int max) {
    int cnt = 0;
    ClockTime ct1;
    time_t in;
    struct tm out;

    MiscTool::getTime(&ct1);

    in = ct1.m_sec;
    memset(&out, 0, sizeof(out));
    localtime_r(&in, &out);
    cnt = snprintf(stamp, max, "<%d-%02d-%02d %02d:%02d:%02d.%03u %u> ",
        out.tm_year + 1900, out.tm_mon + 1,
        out.tm_mday, out.tm_hour,
        out.tm_min, out.tm_sec,
        ct1.m_msec,
        tid);
    if (0 < cnt && cnt < max) {
        return cnt;
    } else {
        stamp[0] = '\0';
        return 0;
    }
}

void Clog::formatLog(int level, const char format[], ...) { 
    char* psz = NULL;
    va_list ap;
    int maxlen = 0;
    int size = 0;
    int cnt = 0;
    int index = 0;
    unsigned tid = 0;
    unsigned slot = 0; 

    if (level > m_log_level || NULL == m_caches[0]) {
        return;
    }

    index = m_curr_index;
    
    tid = MiscTool::getTid();
    slot = tid & MASK_CACHE_INDEX;

    psz = m_caches[slot];
    maxlen = MAX_LOG_CACHE_SIZE - 1;

    cnt = snprintf(psz, maxlen, "[%s]", DEF_LOG_DESC[level]);
    psz += cnt;
    maxlen -= cnt;
    size += cnt;
    
    cnt = preTag(tid, psz, maxlen);
    psz += cnt;
    maxlen -= cnt;
    size += cnt;
    
    va_start(ap, format);
    cnt = vsnprintf(psz, maxlen, format, ap);
    va_end(ap); 
    if (0 < cnt && cnt < maxlen) {
        psz += cnt;
        maxlen -= cnt;
        size += cnt;
    } else if (cnt >= maxlen) {
        cnt = maxlen - 1;
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
        cnt = write(m_log_fd, m_caches[slot], size);
        if (0 < cnt) {
            m_curr_size += cnt; 

            if (isFileMax()) {
                openLog(index + 1, false);
            }
        }
    }
    
    if (m_has_stdout) {
        write(DEF_STDOUT_FD, m_caches[slot], size); 
    } 
}

