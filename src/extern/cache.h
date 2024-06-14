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

    static bool allocBuffer(Buffer* buffer,
        int capacity, PFree pf = NULL);
    
    static void initBuffer(Buffer* buffer);
    static void freeBuffer(Buffer* buffer);
    static void flip(Buffer* buffer);

    static void setCache(Buffer* buffer, Cache* cache);
    static void setSize(Buffer* buffer, int size);
    static void setPos(Buffer* buffer, int pos);
    static void skipPos(Buffer* buffer, int pos);

    static bool isFull(const Buffer* buffer);
    static int leftLen(const Buffer* buffer);
    
    static char* origin(Buffer* buffer);
    static char* curr(Buffer* buffer);

private:
    static bool isValid(Cache* cache);
    static void** align(char* ptr);
};

#endif

