#ifndef __MSGTOOL_H__
#define __MSGTOOL_H__


typedef void (*PFunSig)(int);
struct NodeMsg;

struct ClockTime {
    unsigned m_sec;
    unsigned m_msec;
};

#define INIT_CLOCK_TIME {0,0}

class MsgTool {
public: 
    /* msg functions */
    template <typename T>
    static NodeMsg* creatPreMsg(int size) {
        int preLen = sizeof(T);

        return _creatPreMsg(preLen, size);
    }

    template <typename T>
    static NodeMsg* refPreMsg(NodeMsg* ref) {
        int preLen = sizeof(T);

        return _refPreMsg(preLen, ref);
    }

    template<typename T>
    static T* getPreHead(NodeMsg* pb) {
        char* psz = NULL;
        int preLen = sizeof(T);

        psz = _getPreHead(preLen, pb);
        return reinterpret_cast<T*>(psz);
    } 
    
    static NodeMsg* allocMsg(int size);
    static void freeMsg(NodeMsg* pb);

    static bool completedMsg(NodeMsg* pb);
    static char* getMsg(NodeMsg* pb); 
    static NodeMsg* refNodeMsg(NodeMsg* pb);

    static int getMsgSize(const NodeMsg* pb); 
    static int getMsgPos(const NodeMsg* pb);
    static void setMsgPos(NodeMsg* pb, int pos);
    static void skipMsgPos(NodeMsg* pb, int pos); 

    static void getTime(ClockTime* ct);
    static void pause();
    static int getTid();
    static int getPid();

    static void armSig(int sig, PFunSig fn);
    static void raise(int sig);
    static bool isCoreSig(int sig);
    static int maxSig();

private:
    static char* _getPreHead(int preLen, NodeMsg* pb);

    static NodeMsg* _creatPreMsg(
        int preLen, int size);
    
    static NodeMsg* _refPreMsg(
        int preLen, NodeMsg* ref);
};

#endif

