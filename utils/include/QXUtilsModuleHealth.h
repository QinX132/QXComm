#ifndef _QX_UTIL_MODULE_HEALTH_H_
#define _QX_UTIL_MODULE_HEALTH_H_

#include "QXCommonInclude.h"
#include "QXUtilsLogIO.h"
#include "QXUtilsMsg.h"
#include "QXUtilsThreadPool.h"
#include "QXUtilsMem.h"
#include "QXUtilsTimer.h"
#include "QXUtilsCmdLine.h"

#ifdef __cplusplus
extern "C"{
#endif

#define QX_UTIL_HEALTH_MONITOR_NAME_MAX_LEN                                        QX_BUFF_64

typedef enum _QX_UTIL_MODULES_ENUM
{
    QX_UTIL_MODULES_ENUM_LOG,
    QX_UTIL_MODULES_ENUM_MSG,
    QX_UTIL_MODULES_ENUM_TPOOL,
    QX_UTIL_MODULES_ENUM_CMDLINE,
    QX_UTIL_MODULES_ENUM_MHEALTH,
    QX_UTIL_MODULES_ENUM_MEM,
    QX_UTIL_MODULES_ENUM_TIMER,
    
    QX_UTIL_MODULES_ENUM_MAX
}
QX_UTIL_MODULES_ENUM;

typedef int (*StatReportCB)(char*, int, int*);

typedef struct _MODULE_HEALTH_REPORT_REGISTER
{
    StatReportCB Cb;
    int Interval;
}
MODULE_HEALTH_REPORT_REGISTER;

typedef struct _QX_UTIL_HEALTH_MODULE_INIT_ARG
{
    int LogHealthIntervalS;
    int MsgHealthIntervalS;
    int TPoolHealthIntervalS;
    int CmdLineHealthIntervalS;
    int MHHealthIntervalS;
    int MemHealthIntervalS;
    int TimerHealthIntervalS;
}
QX_UTIL_HEALTH_MODULE_INIT_ARG;

extern MODULE_HEALTH_REPORT_REGISTER sg_ModuleReprt[QX_UTIL_MODULES_ENUM_MAX];

int
QXUtil_HealthModuleExit(
    void
    );

int 
QXUtil_HealthModuleInit(
    QX_UTIL_HEALTH_MODULE_INIT_ARG *InitArg
    );

int
QXUtil_HealthMonitorAdd(
    StatReportCB Cb,
    const char* Name,
    int TimeIntervalS
    );

int
QXUtil_HealthModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );
 
const char*
QXUtil_ModuleNameByEnum(
    int Module
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_MODULE_HEALTH_H_ */
