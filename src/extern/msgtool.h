#ifndef __MSGTOOL_H__
#define __MSGTOOL_H__
#include"cache.h"


struct NodeMsg;

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
    
    static NodeMsg* allocMsg(int size, int prelen = 0);
    static NodeMsg* refNodeMsg(NodeMsg* pb, int prelen = 0);
    static void freeMsg(NodeMsg* pb);

    static void setInfraCb(NodeMsg* pb, PFree pf);
    
    static char* getMsg(NodeMsg* pb);
    static char* getExtraMsg(NodeMsg* pb);

    static bool completedMsg(NodeMsg* pb);
    
    static void setMsgSize(NodeMsg* pb, int size);
    static int getMsgSize(NodeMsg* pb); 
    static int getMsgPos(NodeMsg* pb);
    static void setMsgPos(NodeMsg* pb, int pos);
    static void skipMsgPos(NodeMsg* pb, int pos); 

    static void setExtraCache(NodeMsg* pb, 
        Cache* cahe, int size);
    static Cache* getExtraCache(NodeMsg* pb);
    
    static void setExtraSize(NodeMsg* pb, int size);
    static int getExtraSize(NodeMsg* pb); 
    static int getExtraPos(NodeMsg* pb);
    static void setExtraPos(NodeMsg* pb, int pos);
    static void skipExtraPos(NodeMsg* pb, int pos);

    static int getLeft(NodeMsg* pb);
    static int getExtraLeft(NodeMsg* pb);

private:
    static char* getPreNode(NodeMsg* pb, int preLen); 
};

#endif

