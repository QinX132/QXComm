#ifndef _QX_UTIL_THREAD_POOL_H_
#define _QX_UTIL_THREAD_POOL_H_

#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _QX_UTIL_TPOOL_MODULE_INIT_ARG
{
    int ThreadPoolSize;
    int Timeout;
    int TaskListMaxLength;
}
QX_UTIL_TPOOL_MODULE_INIT_ARG;

int
QXUtil_TPoolModuleInit(
    QX_UTIL_TPOOL_MODULE_INIT_ARG *InitArg
    );

int
QXUtil_TPoolModuleExit(
    void
    );

int
QXUtil_TPoolAddTask(
    void (*TaskFunc)(void*),
    void* TaskArg
    );

int
QXUtil_TPoolAddTaskAndWait(
    void (*TaskFunc)(void*),
    void* TaskArg,
    int32_t TimeoutSec
    );

int
QXUtil_TPoolModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

void 
QXUtil_TPoolSetTimeout(
    uint32_t Timeout
    );

void
QXUtil_TPoolSetMaxQueueLength(
    int32_t QueueLen
    );

#ifdef __cplusplus
}
#endif

#endif /* _QX_UTIL_THREAD_POOL_H_ */
