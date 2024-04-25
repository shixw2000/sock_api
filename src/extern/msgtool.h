#ifndef __MSGTOOL_H__
#define __MSGTOOL_H__


struct NodeMsg;

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

    static void setMsgSize(NodeMsg* pb, int size);
    static int getMsgSize(const NodeMsg* pb); 
    static int getMsgPos(const NodeMsg* pb);
    static void setMsgPos(NodeMsg* pb, int pos);
    static void skipMsgPos(NodeMsg* pb, int pos); 

private:
    static char* _getPreHead(int preLen, NodeMsg* pb);

    static NodeMsg* _creatPreMsg(
        int preLen, int size);
    
    static NodeMsg* _refPreMsg(
        int preLen, NodeMsg* ref);
};

#endif

