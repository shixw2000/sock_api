#ifndef __CTHREAD_H__
#define __CTHREAD_H__


class CThread { 
public:
    CThread();
    virtual ~CThread();

    unsigned long getThr() const {
        return m_thrId;
    }

    static int getTid();

    int start(const char name[]); 
    void stop();
    void join(); 

    bool isRun() const;

    virtual void run() = 0;

private:
    void proc();
    static void* _activate(void* arg); 
    
private:
    volatile bool m_isRun; 
    unsigned long m_thrId;
    char m_name[32];
};

#endif

