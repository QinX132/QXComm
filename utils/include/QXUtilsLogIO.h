#ifndef _QX_UTIL_LOG_IO_H_
#define _QX_UTIL_LOG_IO_H_

#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define QX_ROLE_NAME_MAX_LEN                    128
#define QX_LOF_PATH_MAX_LEN                     256
#define QX_LOG_FILE                             "/tmp/QXlog.log"

#ifdef __GNUC__ 
#define QX_UTIL_FUNC_NAME               __FUNCTION__
#elif defined(_MSC_VER)
#define QX_UTIL_FUNC_NAME               __FUNCSIG__
#else
#define QX_UTIL_FUNC_NAME               __func__
#endif

#define LogInfo(Fmt, ...)               QXUtil_LogPrint(QX_UTIL_LOG_LEVEL_INFO, QX_UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogDbg(Fmt, ...)                QXUtil_LogPrint(QX_UTIL_LOG_LEVEL_DEBUG, QX_UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogWarn(Fmt, ...)               QXUtil_LogPrint(QX_UTIL_LOG_LEVEL_WARNING, QX_UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogErr(Fmt, ...)                QXUtil_LogPrint(QX_UTIL_LOG_LEVEL_ERROR, QX_UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)

#define LogClassInfo(CLASS, Fmt, ...)   QXUtil_LogPrintWithClass(QX_UTIL_LOG_LEVEL_INFO, QX_UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassDbg(CLASS, Fmt, ...)    QXUtil_LogPrintWithClass(QX_UTIL_LOG_LEVEL_DEBUG, QX_UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassWarn(CLASS, Fmt, ...)   QXUtil_LogPrintWithClass(QX_UTIL_LOG_LEVEL_WARNING, QX_UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassErr(CLASS, Fmt, ...)    QXUtil_LogPrintWithClass(QX_UTIL_LOG_LEVEL_ERROR, QX_UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)

typedef enum _QX_UTIL_LOG_LEVEL
{
    QX_UTIL_LOG_LEVEL_INFO,
    QX_UTIL_LOG_LEVEL_DEBUG,
    QX_UTIL_LOG_LEVEL_WARNING,
    QX_UTIL_LOG_LEVEL_ERROR,
    
    QX_UTIL_LOG_LEVEL_MAX
}
QX_UTIL_LOG_LEVEL;

typedef struct _QX_UTIL_LOG_MODULE_INIT_ARG
{
    char LogFilePath[QX_LOF_PATH_MAX_LEN];
    QX_UTIL_LOG_LEVEL LogLevel;
    int LogMaxSize;     // Mb
    int LogMaxNum;
    char RoleName[QX_ROLE_NAME_MAX_LEN];
}
QX_UTIL_LOG_MODULE_INIT_ARG;

int
QXUtil_LogModuleInit(
    QX_UTIL_LOG_MODULE_INIT_ARG *InitArg
    );

void
QXUtil_LogPrint(
    int level,
    const char* Function,
    int Line,
    const char* Fmt,
    ...
    );
void
QXUtil_LogPrintWithClass(
    int Level,
    const char* Function,
    int Line,
    const char* Class,
    const char* Fmt,
    ...
    );
void
QXUtil_LogModuleExit(
    void
    );

int
QXUtil_LogModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

void
QXUtil_LogSetLevel(
    uint32_t LogLevel
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_LOG_IO_H_ */
