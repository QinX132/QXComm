#ifndef _QX_UTIL_MODULE_COMMON_H_
#define _QX_UTIL_MODULE_COMMON_H_

#include "QXCommonInclude.h"
#include "QXUtilsLogIO.h"
#include "QXUtilsMsg.h"
#include "QXUtilsThreadPool.h"
#include "QXUtilsCmdLine.h"
#include "QXUtilsModuleHealth.h"
#include "QXUtilsCrypto.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _QX_UTIL_MODULES_INIT_PARAM
{
    QX_UTIL_CMDLINE_MODULE_INIT_ARG *CmdLineArg;
    QX_UTIL_LOG_MODULE_INIT_ARG *LogArg;
    BOOL InitMsgModule;
    QX_UTIL_HEALTH_MODULE_INIT_ARG *HealthArg;
    QX_UTIL_TPOOL_MODULE_INIT_ARG *TPoolArg;
    BOOL InitTimerModule;
}
QX_UTIL_MODULES_INIT_PARAM;

int
QXUtil_ModuleCommonInit(
    QX_UTIL_MODULES_INIT_PARAM ModuleInitParam
    );

void
QXUtil_ModuleCommonExit(
    void
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_MODULE_COMMON_H_ */
