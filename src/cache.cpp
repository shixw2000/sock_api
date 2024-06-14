#include<cstring>
#include<cstdlib>
#include"shareheader.h"
#include"misc.h"
#include"cache.h"


static const char DEF_DELIM_END_VALID = 0xAD;
static const char DEF_DELIM_END_INVALID = 0xAE;
static const int DEF_ALLOC_ALIGN = 8;
static const int DEF_ALLOC_ALIGN_RES = DEF_ALLOC_ALIGN - 1;
static const int DEF_ALLOC_ALIGN_MASK = ~DEF_ALLOC_ALIGN_RES;

struct Cache {
    PFree m_pf;
    int m_ref;
    int m_cap;
    char m_buf[0];
};
        
void** CacheUtil::align(char* ptr) {
    size_t n = (size_t)ptr;

    n = (n + DEF_ALLOC_ALIGN_RES) & DEF_ALLOC_ALIGN_MASK;
    return (void**)n;
}

char* CacheUtil::mallocAlign(int size) {
    void** pp = NULL;
    char* ptr = NULL; 

    if (0 < size) {
        ptr = (char*)malloc(size + sizeof(void*) + DEF_ALLOC_ALIGN);
        if (NULL != ptr) {
            pp = align(ptr);
            *pp = ptr;
            
            ptr = (char*)(pp + 1);
        }
    }

    return ptr;
}

char* CacheUtil::callocAlign(int nmemb, int size) {
    char* psz = NULL;
    int total = nmemb * size;

    psz = mallocAlign(total);
    if (NULL != psz) {
        MiscTool::bzero(psz, total);
    }
    
    return psz;
}

void CacheUtil::freeAlign(void* ptr) {
    void** pp = NULL;
    char* pb = NULL;
    
    if (NULL != ptr) {
        pp = (void**)ptr;
        pp -= 1;
        pb = (char*)*pp;

        assert(pb < (char*)ptr && (size_t)((char*)ptr - pb) <
            (size_t)(sizeof(void*) + DEF_ALLOC_ALIGN));
        
        free(pb);
    }
}

Cache* CacheUtil::alloc(int capacity, PFree pf) {
    Cache* cache = NULL;

    if (0 < capacity) {
        cache = (Cache*)mallocAlign(sizeof(Cache) + capacity + 1); 
        if (NULL != cache) { 
            MiscTool::bzero(cache, sizeof(*cache));
            
            cache->m_pf = pf;
            cache->m_cap = capacity; 
            cache->m_ref = 1;
            cache->m_buf[capacity] = DEF_DELIM_END_VALID;
        }
    }
    
    return cache; 
}

bool CacheUtil::isValid(Cache* cache) {
    if (DEF_DELIM_END_VALID == 
        cache->m_buf[cache->m_cap]) {
        return true;
    } else {
        LOG_ERROR("****cache_invalid| cache=%p|"
            " capacity=%d| ref=%d| flag=0x%x|",
            cache, cache->m_cap,
            cache->m_ref,
            (unsigned)cache->m_buf[cache->m_cap]);

        return false;
    }
}

void CacheUtil::setCb(Cache* cache, PFree pf) {
    cache->m_pf = pf;
}

void CacheUtil::del(Cache* cache) {
    int ref = 0;
    
    if (NULL != cache) {
        ref = ATOMIC_DEC_FETCH(&cache->m_ref);
        if (0 < ref) {
            return;
        } else {
            if (isValid(cache)) {
                if (NULL != cache->m_pf) {
                    cache->m_pf(cache->m_buf);
                }
                
                cache->m_buf[cache->m_cap] = 
                    DEF_DELIM_END_INVALID;
                
                freeAlign(cache);
            } 
        }
    }
}

Cache* CacheUtil::realloc(Cache* src, int capacity, int used) {
    Cache* dst = NULL;

    if (NULL == src || capacity > src->m_cap) {
        dst = alloc(capacity, src->m_pf);
        if (NULL != dst) {
            if (0 < used) {
                MiscTool::bcopy(dst->m_buf, src->m_buf, used);
            }
            
            del(src);
            return dst;
        } else {
            /* failed */
            return NULL;
        }
    } else {
        return src;
    }
}

Cache* CacheUtil::ref(Cache* cache) {
    int ref = 0;

    if (NULL != cache && isValid(cache)) {
        ref = ATOMIC_INC_FETCH(&cache->m_ref);
        if (1 < ref) {
            return cache;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
} 

char* CacheUtil::data(Cache* cache) {
    if (NULL != cache && isValid(cache)) {
        return cache->m_buf;
    } else {
        return NULL;
    }
}

int CacheUtil::capacity(Cache* cache) {
    if (NULL != cache  && isValid(cache)) {
        return cache->m_cap;
    } else {
        return 0;
    }
}


bool CacheUtil::isFull(const Buffer* buffer) {
    return !(buffer->m_pos < buffer->m_size);
}

int CacheUtil::leftLen(const Buffer* buffer) {
    if (0 <= buffer->m_pos &&
        buffer->m_pos < buffer->m_size) {
        return buffer->m_size - buffer->m_pos;
    } else {
        return 0;
    }
}

void CacheUtil::initBuffer(Buffer* buffer) {
    MiscTool::bzero(buffer, sizeof(*buffer));
}

bool CacheUtil::allocBuffer(Buffer* buffer,
    int capacity, PFree pf) {
    Cache* cache = NULL;

    initBuffer(buffer);

    cache = alloc(capacity, pf);
    if (NULL != cache) {
        buffer->m_cache = cache;
        buffer->m_size = capacity;

        return true;
    } else {
        return false;
    }
}

void CacheUtil::freeBuffer(Buffer* buffer) {
    if (NULL != buffer->m_cache) {
        del(buffer->m_cache);
    }

    MiscTool::bzero(buffer, sizeof(*buffer));
}

void CacheUtil::flip(Buffer* buffer) {
    buffer->m_size = buffer->m_pos;
    buffer->m_pos = 0;
}

void CacheUtil::setCache(Buffer* buffer, Cache* cache) {
    freeBuffer(buffer);

    if (NULL != cache) {
        buffer->m_cache = cache;
        buffer->m_size = CacheUtil::capacity(cache);
    }
}

void CacheUtil::setSize(Buffer* buffer, int size) {
    buffer->m_size = size;
}

void CacheUtil::setPos(Buffer* buffer, int pos) {
    buffer->m_pos = pos;
}

void CacheUtil::skipPos(Buffer* buffer, int pos) {
    buffer->m_pos += pos;
}

char* CacheUtil::origin(Buffer* buffer) {
    return data(buffer->m_cache);
}

char* CacheUtil::curr(Buffer* buffer) {
    char* psz = NULL;

    psz = CacheUtil::data(buffer->m_cache);
    if (NULL != psz && 0 < buffer->m_pos) {
        psz += buffer->m_pos;
    } 

    return psz;
}

