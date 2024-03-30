#ifndef _QX_UTIL_COMMON_UTIL_H_
#define _QX_UTIL_COMMON_UTIL_H_
#include "QXCommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define QX_UTIL_GET_CPU_USAGE_START                     \
    do { \
        uint64_t _QX_UTIL_TOTAL_CPU_START = 0, _QX_UTIL_TOTAL_CPU_END = 0; \
        uint64_t _QX_UTIL_IDLE_CPU_START = 0, _QX_UTIL_IDLE_CPU_END = 0; \
        (void)QXUtil_GetCpuTime(&_QX_UTIL_TOTAL_CPU_START, &_QX_UTIL_IDLE_CPU_START);

#define QX_UTIL_GET_CPU_USAGE_END(_CPU_USAGE_)                       \
        (void)QXUtil_GetCpuTime(&_QX_UTIL_TOTAL_CPU_END, &_QX_UTIL_IDLE_CPU_END); \
        _CPU_USAGE_ = _QX_UTIL_TOTAL_CPU_END > _QX_UTIL_TOTAL_CPU_START ? \
                    1.0 - (double)(_QX_UTIL_IDLE_CPU_END - _QX_UTIL_IDLE_CPU_START) / \
                                (_QX_UTIL_TOTAL_CPU_END - _QX_UTIL_TOTAL_CPU_START) : 0; \
    }while(0);

void
QXUtil_MakeDaemon(
    void
    );

int
QXUtil_OpenPidFile(
    char* Path
    );

int
QXUtil_IsProcessRunning(
    int Fd
    );

int
QXUtil_SetPidIntoFile(
    int Fd
    );

void 
QXUtil_CloseStdFds(
    void
    );

int
QXUtil_GetCpuTime(
    uint64_t *TotalTime,
    uint64_t *IdleTime
    );

uint64_t 
QXUtil_htonll(
    uint64_t value
    );

uint64_t 
QXUtil_ntohll(
    uint64_t value
    );

void
QXUtil_ChangeCharA2B(
    char* String,
    size_t StringLen,
    char A,
    char B
    );

int
QXUtil_ParseStringToIpv4(
    const char* String,
    size_t StringLen,
    uint32_t *Ip
    );
int
QXUtil_ParseStringToIpv4AndPort(
    const char* String,
    size_t StringLen,
    uint32_t *Ip,
    uint16_t *Port
    );
#ifdef __cplusplus
 }
#endif

#endif /* _QX_UTIL_COMMON_UTIL_H_ */
