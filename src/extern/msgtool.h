#ifndef __MSGTOOL_H__
#define __MSGTOOL_H__
#include"shareheader.h"


class MsgTool {
public: 
    /* msg functions */
    template <typename T>
    static NodeMsg* creatPreMsg(int size) {
        return allocMsg(size, sizeof(T));
    }

    template <typename T>
    static NodeMsg* refPreMsg(NodeMsg* ref) {
        return refNodeMsg(ref, sizeof(T));
    }

    template<typename T>
    static T* getPreHead(NodeMsg* pb) {
        char* psz = NULL;

        psz = getPreNode(pb, sizeof(T));
        return reinterpret_cast<T*>(psz);
    } 
    
    static NodeMsg* allocNode(int prelen = 0);
    
    static NodeMsg* allocMsg(int size, int prelen = 0);
    static NodeMsg* refNodeMsg(NodeMsg* pb, int prelen = 0);
    static void freeMsg(NodeMsg* pb);

    static char* getMsg(NodeMsg* pb, bool ext = false);
    static char* getCurr(NodeMsg* pb, bool ext = false);
    static Buffer* getBuffer(NodeMsg* pb, bool ext = false);

    /* set size == pos, and pos = 0 */
    static void flip(NodeMsg* pb, bool ext = false);

    static void setCache(NodeMsg* pb, Cache* cahe,
        int size, bool ext = false);
    static void setCb(NodeMsg* pb, 
        PFree pf, bool ext = false);
    
    static void setMsgSize(NodeMsg* pb, int size, bool ext = false);
    static void setMsgPos(NodeMsg* pb, int pos, bool ext = false);
    static void skipMsgPos(NodeMsg* pb, int pos, bool ext = false);
    static int getMsgSize(NodeMsg* pb, bool ext = false); 
    static int getMsgPos(NodeMsg* pb, bool ext = false);
    static int getLeft(NodeMsg* pb, bool ext = false);

    static bool completedMsg(NodeMsg* pb);
  
    static NodeMsg* allocUdpMsg(int size);
    static NodeMsg* refUdpMsg(NodeMsg* msg);

    static int setUdpAddr(NodeMsg* msg, const SockAddr& addr);
    static const SockAddr* getUdpAddr(NodeMsg* msg);

private:
    static char* getPreNode(NodeMsg* pb, int preLen); 
};

#endif

