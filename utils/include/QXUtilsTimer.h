#ifndef _QX_UTIL_MODULE_TIMER_H_
#define _QX_UTIL_MODULE_TIMER_H_

#include "QXCommonInclude.h"
#include "QXUtilsList.h"
#include "QXUtilsModuleHealth.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef enum _QX_UTIL_TIMER_TYPE
{
    QX_UTIL_TIMER_TYPE_ONE_TIME,
    QX_UTIL_TIMER_TYPE_LOOP,
    
    QX_UTIL_TIMER_TYPE_MAX_UNSPEC
}
QX_UTIL_TIMER_TYPE; 

typedef void (*TimerCB)(evutil_socket_t, short, void*);

typedef struct _QX_UTIL_TIMER_EVENT_NODE
{
    struct event* Event;
    TimerCB Cb;
    void* Arg;
    uint32_t IntervalMs;
    QX_LIST_NODE List;
}
QX_UTIL_TIMER_EVENT_NODE;

typedef QX_UTIL_TIMER_EVENT_NODE* TIMER_HANDLE;

int
QXUtil_TimerModuleExit(
    void
    );

int 
QXUtil_TimerModuleInit(
    void
    );

int
QXUtil_TimerAdd(
    TimerCB Cb,
    uint32_t IntervalMs,
    void* Arg,
    QX_UTIL_TIMER_TYPE TimerType,
    __out TIMER_HANDLE *TimerHandle
    );
 
void
QXUtil_TimerDel(
    __inout TIMER_HANDLE *TimerHandle
    );

int
QXUtil_TimerModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_MODULE_TIMER_H_ */
