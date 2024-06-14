#ifndef __NODEBASE_H__
#define __NODEBASE_H__


struct LList;
struct NodeMsg;
struct Cache;
struct Buffer; 

class NodeUtil {
public: 
    static NodeMsg* creatNode(int prelen = 0);
    
    static void freeNode(NodeMsg* pb);

    static NodeMsg* refNode(NodeMsg* pb, int prelen = 0);
    
    static char* getPreNode(NodeMsg* pb, int preLen);

    static Buffer* getBuffer(NodeMsg* pb, bool ext = false); 

    static void setNodeType(NodeMsg* pb, int type);
    static int getNodeType(NodeMsg* pb);
    
    static bool isCompleted(NodeMsg* pb);
    
    static void queue(LList* root, NodeMsg* pb);

    static NodeMsg* toNode(LList* node); 
};

#endif

