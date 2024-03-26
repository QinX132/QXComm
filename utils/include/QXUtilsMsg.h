#ifndef _QX_UTIL_MSG_H_
#define _QX_UTIL_MSG_H_
#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define QX_UTIL_MSG_CONTENT_MAX_LEN                                    (1024 * 1024)
#define QX_UTIL_MSG_VER(_M_VER_, _F_VER_, _R_VER_)                     (_M_VER_ << 5 | _F_VER_ << 2 | _R_VER_)
                                                    // m_ver 3bits max 7, f_ver 2bits max 3, r_ver 3 bits max 7
#define QX_UTIL_MSG_VER_MAGIC                                          QX_UTIL_MSG_VER(1, 0, 0)

// total 40 Bytes
typedef struct _QX_UTIL_MSG_HEAD{
    uint8_t VerMagic;               // must be the first
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t IsMsgEnd : 1;
    uint8_t Reserved_1 : 7;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t Reserved_1 : 7;
    uint8_t IsMsgEnd : 1;
#else
    #error "unknown byte order!"
#endif
    uint16_t Type;
    uint32_t ContentLen;
    uint32_t SessionId;
    uint32_t ClientId;
    uint8_t Reserved_2[24];
}
QX_UTIL_MSG_HEAD;

typedef struct _QX_UTIL_MSG_CONT
{
    uint8_t VarLenCont[QX_UTIL_MSG_CONTENT_MAX_LEN];
}
QX_UTIL_MSG_CONT;

// total 88 Bytes
typedef struct _QX_UTIL_MSG_TAIL
{
    uint8_t Reserved[16];
    uint64_t TimeStamp;         // ms timestamp
    uint8_t Sign[64];
}
QX_UTIL_MSG_TAIL;
// head + tail = 128 Bytes

typedef struct _QX_UTIL_MSG
{
    QX_UTIL_MSG_HEAD Head;
    QX_UTIL_MSG_CONT Cont;
    QX_UTIL_MSG_TAIL Tail;
}
QX_UTIL_MSG;

int
QXUtil_MsgModuleInit(
    void
    );

int
QXUtil_MsgModuleExit(
    void
    );

int
QXUtil_MsgModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
QXUtil_RecvMsg(
    int Fd,
    __inout QX_UTIL_MSG *RetMsg
    );

MUST_CHECK
QX_UTIL_MSG *
QXUtil_NewMsg(
    void
    );

void
QXUtil_FreeMsg(
    QX_UTIL_MSG *Msg
    );

int
QXUtil_SendMsg(
    int Fd,
    QX_UTIL_MSG *Msg
    );

int
QXUtil_FillMsgCont(
    QX_UTIL_MSG *Msg,
    void* FillCont,
    size_t FillContLen
    );

void
QXUtil_ClearMsgCont(
    QX_UTIL_MSG *Msg
    );

#ifdef __cplusplus
}
#endif

#endif /* _QX_UTIL_MSG_H_ */
