#include<cstring>
#include<cstdlib>
#include<cstdio>
#include"config.h"
#include"shareheader.h"


int Config::strip(char* text, int max) {
    char* beg = NULL;
    char* end = NULL;
    int len = 0;

    len = strnlen(text, max);
    beg = text;
    end = text + len;

    while (isspace(*beg) && beg < end) {
        ++beg;
    }

    while (beg < end && isspace(*(end-1))) {
        --end;
    }

    if (beg < end) {
        len = end - beg;
        if (beg != text) {
            memmove(text, beg, len);
        }
    } else {
        len = 0;
    }

    text[len] = '\0'; 
    return len;
}

bool Config::isIgnore(const char* line, int len) { 
    return 0 >= len || '\0' == line[0] || '#' == line[0];
}

bool Config::isSection(const char* line, int len) { 
    return 2 < len && '[' == line[0] && ']' == line[len-1];
}

bool Config::isToken(const char* line, int len) { 
    return 2 <= len && (
        ('\'' == line[0] && '\'' == line[len-1]) ||
        ('\"' == line[0] && '\"' == line[len-1]));
}

int Config::parseFile(const char* conf) {
    static const int MAX_LINE_LEN = 512;
    FILE* hd = NULL;
    char* psz = NULL;
    Section* sec = NULL;
    int ret = 0;
    int max = 0;
    int line_no = 0;
    char line[MAX_LINE_LEN] = {0};

    if (NULL != conf && '\0' != conf[0]) {
        snprintf(m_name, sizeof(m_name), "%s", conf);
    } else {
        snprintf(m_name, sizeof(m_name), "%s", DEF_CONF_FILE);
    }
    
    clear();

    hd = fopen(m_name, "rb");
    if (NULL != hd) {
        while (!feof(hd) && 0 == ret) {
            psz = fgets(line, MAX_LINE_LEN, hd);
            if (NULL != psz) {
                ++line_no;
            } else {
                break;
            }

            max = strip(line, MAX_LINE_LEN);
            if (isIgnore(line, max)) {
                continue;
            } else if (isSection(line, max)) {
                ret = parseSec(m_name, line_no, &sec, line, max);
            } else if (NULL != sec) {
                ret = parseKey(m_name, line_no, sec, line, max);
            } else {
                LOG_ERROR("parse_line| conf_file=%s|"
                    " line_no=%d| msg=invalid line|", 
                    m_name, line_no);
                ret = -2;
            } 
        }

        fclose(hd);
    } else {
        LOG_ERROR("open_file| conf_file=%s|"
            " msg=invalid path|", m_name);
        ret = -1;
    }

    return ret;
}

int Config::getAddr(AddrInfo& addr, const typeStr& sec) {
    int ret = 0;

    ret = getNum(sec, KEY_PORT, addr.m_port);
    CHK_RET(ret);

    ret = getToken(sec, KEY_IP, addr.m_ip);
    CHK_RET(ret);

    return ret;
}

int Config::parseSec(const char* file, int line_no,
    Section** ppsec, char* line, int max) {
    char* psz = line;
    int ret = 0;
    int len = 0;
    typeStr name;

    ++psz;
    max -= 2;

    len = strip(psz, max);
    if (0 < len) {
        name.assign(psz, len);
        Section& sec = m_sections[name];

        sec.m_name = name;
        *ppsec = &sec;
    } else {
        ret = -1;
        *ppsec = NULL;
        LOG_ERROR("parse_sec| conf_file=%s| line_no=%d|"
            " msg=invalid line|", 
            file, line_no);
    }
    
    return ret;
}

int Config::parseKey(const char* file, int line_no,
    Section* sec, char* line, int max) {
    char* psz = NULL;
    char* end = NULL;
    int ret = 0;
    int vlen = 0;
    int klen = 0;
    int num = 0;
    typeStr key;
    typeStr token;

    end = line + max;
    psz = strchr(line, '=');
    if (NULL != psz) {
        klen = psz - line;
        klen = strip(line, klen);

        if (0 < klen) {
            key.assign(line, klen);
            ++psz;
            max = end - psz;
            
            vlen = strip(psz, max);
            if (0 < vlen) {
                if (isToken(psz, vlen)) {
                    token.assign(psz+1, vlen-2);
                    sec->m_tokens[key] = token;
                } else {
                    num = strtol(psz, &end, 0);
                    if (end == psz + vlen) {
                        /* num */
                        sec->m_nums[key] = num; 
                    } else {
                        token.assign(psz, vlen);
                        sec->m_tokens[key] = token;
                    }
                }

                return 0;
            } else {
                ret = -3;
            }
        } else {
            ret = -2;
        }
    } else {
        ret = -1;
    }

    LOG_ERROR("parse_key| conf_file=%s| line_no=%d|"
        " ret=%d| msg=invalid line|", 
        file, line_no, ret);
    return ret;
}

int Config::getToken(const typeStr& sec, 
    const typeStr& key, typeStr& v) {
    int ret = 0;
    typeMapSecItrConst itr = m_sections.find(sec);

    v.clear();
    
    if (m_sections.end() != itr) {
        const Section& section = itr->second;
        typeMapStrItrConst itrSub = section.m_tokens.find(key);

        if (section.m_tokens.end() != itrSub) {
            v = itrSub->second;
            ret = 0;
        } else {
            LOG_ERROR("get_token| conf_file=%s| sec=%s| key=%s|"
                " msg=key of string not exists|", 
                m_name, sec.c_str(), key.c_str());
            ret = -2;
        }
    } else {
        LOG_ERROR("get_token| conf_file=%s| sec=%s| key=%s|"
            " msg=sec not exists|", 
            m_name, sec.c_str(), key.c_str());
        ret = -1;
    }

    return ret;
}

int Config::getNum(const typeStr& sec, 
    const typeStr& key, int& n) {
    int ret = 0;
    typeMapSecItrConst itr = m_sections.find(sec);

    n = 0;
    
    if (m_sections.end() != itr) {
        const Section& section = itr->second;
        typeMapNumItrConst itrSub = section.m_nums.find(key);

        if (section.m_nums.end() != itrSub) {
            n = itrSub->second;
            ret = 0;
        } else {
            LOG_ERROR("get_num| conf_file=%s| sec=%s| key=%s|"
                " msg=key of number not exists|", 
                m_name, sec.c_str(), key.c_str());
            ret = -2;
        }
    } else {
        LOG_ERROR("get_num| conf_file=%s| sec=%s| key=%s|"
            " msg=sec not exists|", 
            m_name, sec.c_str(), key.c_str());
        ret = -1;
    }

    return ret;
}

typeStr Config::getIndexName(const typeStr& name, int n) {
    char buf[64] = {0};

    snprintf(buf, sizeof(buf), "%s%d", 
        name.c_str(), n);
    return buf;
}

typeStr Config::getCntKey(const typeStr& name) {
    return name + ITEM_SUFFIX;
}

int Config::getItemCnt(const typeStr& sec, 
    const typeStr& key, int& n) {
    int ret = 0;
    typeStr name;
    typeMapSecItrConst itr;
    typeMapNumItrConst itrSub;

    n = 0;
    
    itr = m_sections.find(sec);
    if (m_sections.end() != itr) { 
        const Section& section = itr->second;
        
        name = getCntKey(key); 
        itrSub = section.m_nums.find(name); 
        if (section.m_nums.end() != itrSub) {
            n = itrSub->second;
            ret = 0;
        } else {
            LOG_ERROR("get_item_cnt| conf_file=%s| sec=%s| key=%s|"
                " msg=key of number not exists|", 
                m_name, sec.c_str(), name.c_str());
            
            ret = -2;
        }
    } else {
        LOG_ERROR("get_item_cnt| conf_file=%s| sec=%s| key=%s|"
            " msg=sec not exists|", 
            m_name, sec.c_str(), name.c_str());
        ret = -1;
    }

    return ret;
}

bool Config::existSec(const typeStr& sec) {
    typeMapSecItrConst itr = m_sections.find(sec);

    return m_sections.end() != itr;
}

void Config::clear() {
    m_sections.clear();
}

