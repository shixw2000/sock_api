#ifndef __CMDCACHE_H__
#define __CMDCACHE_H__


enum EnumNodeType {
    ENUM_NODE_APP = 0,
    ENUM_NODE_SOCK_EXIT = 100,
};

struct NodeMsg; 

class CmdUtil {
public:
    /* cmd functions */
    template<typename T>
    static NodeMsg* creatCmd(int cmd) {
        NodeMsg* pb = NULL;
        
        pb = creatNodeCmd(cmd, sizeof(T));
        return pb;
    } 

    template<typename T>
    static T* getCmdBody(NodeMsg* pb) {
        void* body = NULL;

        body = getBody(pb); 
        return reinterpret_cast<T*>(body);
    } 
    
    static int getCmd(NodeMsg* pb);

    static NodeMsg* creatSockExitMsg();
    static bool isSockExit(NodeMsg* msg);

private:
    static void* getBody(NodeMsg* pb); 
    static NodeMsg* creatNodeCmd(int cmd, int size); 
};

#endif

