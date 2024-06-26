#include<unistd.h>
#include<sys/types.h>
#include<pthread.h>
#include<cstring>
#include"shareheader.h"
#include"cthread.h"
#include"misc.h"


CThread::CThread() {
    m_thrId = 0;
    m_isRun = false;
    MiscTool::bzero(m_name, sizeof(m_name));
}

CThread::~CThread() {
}

int CThread::start(const char name[]) {
    int ret = 0;
    pthread_attr_t attr;
    pthread_t thr = 0;

    MiscTool::bzero(m_name, sizeof(m_name));
    MiscTool::strCpy(m_name, name, sizeof(m_name));

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ret = pthread_create(&thr, &attr, &CThread::_activate, this);
    if (0 == ret) {
        /* ok */
        m_thrId = (unsigned long)thr;
    } else {
        m_thrId = 0;
        ret = -1;
    }
    
    pthread_attr_destroy(&attr); 
    return ret;
}

void CThread::stop() {
    m_isRun = false;
}

void CThread::join() {
    if (0 != m_thrId) { 
        pthread_join(m_thrId, NULL);
        m_thrId = 0;
    }
}

bool CThread::isRun() const {
    return m_isRun;
}

void* CThread::_activate(void* arg) {
    CThread* pthr = reinterpret_cast<CThread*>(arg);
    
    pthr->proc(); 
    return (void*)NULL;
}

void CThread::proc() { 
    LOG_INFO("start to run| name=%s| tid=%d| id=0x%x|", 
        m_name, MiscTool::getTid(), m_thrId);

    m_isRun = true;
    run(); 
    m_isRun = false;
    
    LOG_INFO("end to run| name=%s| tid=%d| id=0x%lx|", 
        m_name, MiscTool::getTid(), m_thrId);
}

