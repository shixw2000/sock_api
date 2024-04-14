#ifndef __MSGUTIL_H__
#define __MSGUTIL_H__
#include"sockdata.h"


struct NodeCmd;
struct NodeMsg;

class MsgUtil {
public: 
    /* cmd functions */
    template<typename T>
    static NodeCmd* creatCmd(int cmd) {
        NodeCmd* pCmd = NULL;
        CmdHead_t* ph = NULL;
        int size = sizeof(CmdHead_t) + sizeof(T);
        
        pCmd = creatNodeCmd(size);
        ph = getCmd(pCmd);

        ph->m_cmd = cmd;
        return pCmd;
    } 

    template<typename T>
    static T* getCmdBody(NodeCmd* pCmd) {
        CmdHead_t* ph = getCmd(pCmd);

        return reinterpret_cast<T*>(ph->m_body);
    }

    static NodeCmd* creatNodeCmd(int size);
    
    static void freeNodeCmd(NodeCmd* pb);

    static CmdHead_t* getCmd(const NodeCmd* pb); 
    
    static NodeCmd* toNodeCmd(LList* node);
    
    static void addNodeCmd(LList* root, NodeCmd* pb); 
    static NodeCmd* refNodeCmd(NodeCmd* pb); 


    /* msg functions */ 
    static NodeMsg* creatNodeMsg(int size);
    static NodeMsg* refNodeMsg(NodeMsg* pb); 
    
    static void freeNodeMsg(NodeMsg* pb); 
    
    static char* getMsg(NodeMsg* pb); 
    static bool completedMsg(NodeMsg* pb);
    
    static void setMsgSize(NodeMsg* pb, int size);
    static int getMsgSize(const NodeMsg* pb); 
    static int getMsgPos(const NodeMsg* pb);
    static void setMsgPos(NodeMsg* pb, int pos);
    static void skipMsgPos(NodeMsg* pb, int pos);
    
    static void addNodeMsg(LList* root, NodeMsg* pb);
    static NodeMsg* toNodeMsg(LList* node); 

    static char* _getPreHead(int preLen, NodeMsg* pb);

    static NodeMsg* _creatPreNodeMsg(
        int preLen, int size);
    
    static NodeMsg* _refPreNodeMsg(
        int preLen, NodeMsg* ref);
    
    static int _alignment(int size); 
};

#endif

