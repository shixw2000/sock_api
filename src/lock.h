#ifndef __LOCK_H__
#define __LOCK_H__
#include<pthread.h>


class Lock {
public:
    virtual ~Lock() {}
    
    virtual bool lock() = 0;
    virtual bool unlock() = 0;
};

class SpinLock : public Lock {
public:
    SpinLock();
    ~SpinLock();

    virtual bool lock();
    virtual bool unlock();

private:
    pthread_spinlock_t m_lock;
};

class MutexLock : public Lock {
public:
    MutexLock();
    ~MutexLock(); 
    
    virtual bool lock();
    virtual bool unlock();

protected:
    pthread_mutex_t m_mutex;
};

class MutexCond : public MutexLock {
public:
    MutexCond();
    ~MutexCond();
    
    virtual bool signal();
    virtual bool broadcast();
    virtual bool wait();

private:
    pthread_cond_t m_cond;
};

#endif

