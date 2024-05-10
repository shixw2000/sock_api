#ifndef __NODEBASE_H__
#define __NODEBASE_H__


struct LList;
struct NodeMsg;
struct Cache;
struct NodeInfo; 

enum EnumNodeType {
    ENUM_NODE_INFR = 0,
    ENUM_NODE_VAR,

    ENUM_NODE_TYPE_END
};

class NodeUtil {
public: 
    static NodeMsg* creatNode(int prelen = 0);
    
    static void freeNode(NodeMsg* pb);

    static NodeMsg* refNode(NodeMsg* pb, int prelen = 0);
    
    static char* getPreNode(NodeMsg* pb, int preLen);

    static NodeInfo* getNodeInfo(NodeMsg* pb, int type); 
    
    static void setInfo(NodeInfo* pinfo,
        Cache* cache, int size, int pos = 0);
    
    static void setCache(NodeMsg* pb, int type, 
        Cache* cache, int size); 
    
    static void setSize(NodeMsg* pb, int type, int size); 
    static void setPos(NodeMsg* pb, int type, int pos);
    static void skipPos(NodeMsg* pb, int type, int pos);

    static Cache* getCache(NodeMsg* pb, int type);
    static int getSize(NodeMsg* pb, int type);
    static int getPos(NodeMsg* pb, int type); 
    static int getLeft(NodeMsg* pb, int type);

    static bool isCompleted(NodeMsg* pb, int type); 
    
    static void queue(LList* root, NodeMsg* pb);

    static NodeMsg* toNode(LList* node);
};

#endif

