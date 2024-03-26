#ifndef _QX_UTIL_MEM_H_
#define _QX_UTIL_MEM_H_

#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define QX_UTIL_MEM_MODULE_INVALID_ID                                   -1

/*  After the initialization process, you can utilize the "register" and "unregister" functions to 
 *  manage separate memids independently. Later, you can use "MemCalloc" and "MemFree" functions 
 *  for unified memid management. However, for the sake of convenience, I will handle the management 
 *  with a unified memid, using MyCalloc and MyFree.
 */

int
QXUtil_MemModuleInit(
    void
    );

int
QXUtil_MemModuleExit(
    void
    );

int
QXUtil_MemModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
QXUtil_MemRegister(
    int *MemId,
    char *Name
    );

int
QXUtil_MemUnRegister(
    int* MemId
    );


void*
QXUtil_MemCalloc(
    int MemId,
    size_t Size
    );

void
QXUtil_MemFree(
    int MemId,
    void* Ptr
    );

void*
QXUtil_QXCalloc(
    size_t Size
    );

void
QXUtil_QXFree(
    void* Ptr
    );

BOOL
QXUtil_MemLeakSafetyCheck(
    void
    );

BOOL
QXUtil_MemLeakSafetyCheckWithId(
    int MemId
    );

#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_MEM_H_ */
