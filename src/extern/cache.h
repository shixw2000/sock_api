#ifndef __CACHE_H__
#define __CACHE_H__
#include"shareheader.h"


class CacheUtil {
public: 
    static char* mallocAlign(int size);
    static char* callocAlign(int nmemb, int size);
    static void freeAlign(void* ptr);
    
    static Cache* alloc(int capacity, PFree pf = NULL);
    static void del(Cache* cache);
    static Cache* realloc(Cache* src, int capacity, int used);

    static void setCb(Cache* cache, PFree pf);
    
    static Cache* ref(Cache* cache);
    static char* data(Cache* cache);
    static int capacity(Cache* cache); 

private:
    static bool isValid(Cache* cache);
    static char** align(char* ptr);
};

#endif

