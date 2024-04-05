#include<errno.h>
#include<cstring>
#include"lock.h"
#include"shareheader.h"


#define ERR_MSG() strerror(errno)

SpinLock::SpinLock() {
    pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
}

SpinLock::~SpinLock() {
    pthread_spin_destroy(&m_lock);
}

bool SpinLock::lock() {
    int ret = 0;

    ret = pthread_spin_lock(&m_lock);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("spin_lock| ret=%d| error=%s|",
            ret, ERR_MSG());
        return false;
    }
}

bool SpinLock::unlock() {
    int ret = 0;

    ret = pthread_spin_unlock(&m_lock);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("spin_unlock| ret=%d| error=%s|", 
            ret, ERR_MSG());
        return false;
    }
}


MutexLock::MutexLock() {
    pthread_mutex_init(&m_mutex, NULL);
}

MutexLock::~MutexLock() {
    pthread_mutex_destroy(&m_mutex);
}

bool MutexLock::lock() {
    int ret = 0;

    ret = pthread_mutex_lock(&m_mutex);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("mutex_lock| ret=%d| error=%s|", 
            ret, ERR_MSG());
        return false;
    }
}

bool MutexLock::unlock() {
    int ret = 0;

    ret = pthread_mutex_unlock(&m_mutex);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("mutex_unlock| ret=%d| error=%s|",
            ret, ERR_MSG());
        return false;
    }
}

MutexCond::MutexCond() {
    pthread_cond_init(&m_cond, NULL);
}

MutexCond::~MutexCond() {
    pthread_cond_destroy(&m_cond);
}

bool MutexCond::signal() {
    int ret = 0;

    ret = pthread_cond_signal(&m_cond);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("mutex_cond_signal| ret=%d| error=%s|", ret, ERR_MSG());
        return false;
    }
}

bool MutexCond::broadcast() {
    int ret = 0;

    ret = pthread_cond_broadcast(&m_cond);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("mutex_cond_broadcast| ret=%d| error=%s|", ret, ERR_MSG());
        return false;
    }
}

bool MutexCond::wait() {
    int ret = 0;

    ret = pthread_cond_wait(&m_cond, &m_mutex);
    if (0 == ret) {
        return true;
    } else {
        LOG_ERROR("mutex_cond_wait| ret=%d| error=%s|", ret, ERR_MSG());
        return false;
    }
}


