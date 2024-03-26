#ifndef _QX_UTIL_CMD_LINE_H_
#define _QX_UTIL_CMD_LINE_H_

#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*ExitHandle)(void);
typedef int (*CmdExternalFn)(char*, size_t, char*, size_t); // inBuff-Cmd inBuffLen-Cmdlen outBuff outBuffMaxSize

typedef struct _QX_UTIL_CMD_EXTERNAL_CONT
{
    char Opt[QX_BUFF_64];
    char Help[QX_BUFF_256];
    int Argc;
    CmdExternalFn Cb;
}
QX_UTIL_CMD_EXTERNAL_CONT;

typedef struct _QX_UTIL_CMDLINE_MODULE_INIT_ARG
{
    char RoleName[QX_BUFF_64];
    int Argc;
    char** Argv;
    ExitHandle ExitFunc;
}
QX_UTIL_CMDLINE_MODULE_INIT_ARG;

int
QXUtil_CmdLineModuleInit(
    QX_UTIL_CMDLINE_MODULE_INIT_ARG *InitArg
    );

void
QXUtil_CmdLineModuleExit(
    void
    );

int
QXUtil_CmdLineModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
QXUtil_CmdExternalRegister(
    __in QX_UTIL_CMD_EXTERNAL_CONT Cont
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_CMD_LINE_H_ */
