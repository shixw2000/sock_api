#ifndef __CONFIG_H__
#define __CONFIG_H__
#include<map>
#include<vector>
#include<string>


#define CHK_RET(x) if (0 != (x)) return (x) 
static const char ITEM_SUFFIX[] = "_cnt";
static const char GLOBAL_SEC[] = "global";
static const char KEY_IP[] = "ip";
static const char KEY_PORT[] = "port";
static const char DEF_CONF_FILE[] = "service.conf";
static const char DEF_KEY_LOG_DIR[] = "log_dir";
static const char DEF_KEY_LOG_LEVEL_NAME[] = "log_level";
static const char DEF_KEY_LOG_STDIN_NAME[] = "log_stdin";
static const char DEF_KEY_LOG_FILE_SIZE[] = "log_size";

typedef std::string typeStr;

struct AddrInfo {
    int m_port;
    typeStr m_ip;
};

class Config { 
    typedef std::map<typeStr, int> typeMapNum;
    typedef typeMapNum::iterator typeMapNumItr;
    typedef typeMapNum::const_iterator typeMapNumItrConst;
    
    typedef std::map<typeStr, typeStr> typeMapStr;
    typedef typeMapStr::iterator typeMapStrItr;
    typedef typeMapStr::const_iterator typeMapStrItrConst; 

    struct Section {
        typeStr m_name;
        typeMapNum m_nums;
        typeMapStr m_tokens;
    };
    
    typedef std::map<typeStr, Section> typeMapSec;
    typedef typeMapSec::iterator typeMapSecItr;
    typedef typeMapSec::const_iterator typeMapSecItrConst; 
    
public: 
    int parseFile(const char* file);

    bool existSec(const typeStr& sec);

    int getToken(const typeStr& sec, 
        const typeStr& key, typeStr& v);

    int getNum(const typeStr& sec, 
        const typeStr& key, int& n);

    int getItemCnt(const typeStr& sec, 
        const typeStr& key, int& n);

    typeStr getIndexName(const typeStr& name, int n); 

    int strip(char* text, int max); 

    int getAddr(AddrInfo& addr, const typeStr& sec);

private: 
    void clear(); 
    
    int parseSec(const char* file, int line_no,
    Section** ppsec, char* line, int max);

    int parseKey(const char* file, int line_no,
    Section* sec, char* line, int max);

    bool isIgnore(const char* line, int len);

    bool isSection(const char* line, int len);

    bool isToken(const char* line, int len);

    typeStr getCntKey(const typeStr& name);

private:
    typeMapSec m_sections;
    char m_name[256];
};

#endif

