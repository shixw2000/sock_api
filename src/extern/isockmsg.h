#ifndef __ISOCKMSG_H__
#define __ISOCKMSG_H__


/* message from here alignment with 1 byte */
#pragma pack(push, 1)


struct MsgHead_t {
    int m_size;
    unsigned m_seq;
    unsigned m_crc;
    unsigned m_retcode;
    unsigned short m_version;
    unsigned short m_cmd;
    char m_body[0];
};

#pragma pack(pop)

static const int DEF_MSG_HEAD_SIZE = sizeof(MsgHead_t);

struct NodeMsg;

struct SockBuffer {
    NodeMsg* m_msg; 
    unsigned m_pos;
    char m_head[DEF_MSG_HEAD_SIZE];
};

#endif

