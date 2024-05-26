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
        
char** CacheUtil::align(char* ptr) {
    size_t n = (size_t)ptr;

    n = (n + DEF_ALLOC_ALIGN_RES) & DEF_ALLOC_ALIGN_MASK;
    return (char**)n;
}

char* CacheUtil::mallocAlign(int size) {
    char** pp = NULL;
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
    char* psz = NULL;
    
    if (NULL != ptr) {
        psz = ((char**)ptr)[-1];
        
        free(psz);
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

    if (capacity > src->m_cap) {
        dst = alloc(capacity, src->m_pf);
        if (NULL != dst) {
            if (0 < used) {
                MiscTool::bcopy(dst->m_buf, src->m_buf, used);
            }
            
            del(src);
        } else {
            /*failed, then do nothing */
        }
    } else {
        dst = src;
    }

    return dst;
}

Cache* CacheUtil::ref(Cache* cache) {
    int ref = 0;

    if (isValid(cache)) {
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
    if (isValid(cache)) {
        return cache->m_buf;
    } else {
        return NULL;
    }
}

int CacheUtil::capacity(Cache* cache) {
    return cache->m_cap;
}

