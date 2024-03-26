#include "QXUtilsMem.h"
#include "QXUtilsLogIO.h"
#include "QXUtilsList.h"
#include "QXUtilsModuleHealth.h"

#define QX_MEM_MODULE_MAX_NUM                                   64

typedef struct _QX_MEM_NODE
{
    BOOL Registered;
    uint8_t MemModuleId;
    char MemModuleName[QX_BUFF_32];
    uint64_t MemBytesAlloced;
    uint64_t MemBytesFreed;
    pthread_spinlock_t MemSpinlock;
}
QX_MEM_NODE;

typedef struct _QX_MEM_PREFIX
{
    uint8_t     MemModuleId;
    uint32_t    Size    : 31;
    uint32_t    Freed   : 1;
}//__attribute__((packed))  // This will lead to serious problems related to locks
QX_MEM_PREFIX;

typedef struct _QX_MEM_WORKER
{
    QX_MEM_NODE Nodes[QX_MEM_MODULE_MAX_NUM];
    int MainModId;
    BOOL Inited;
    pthread_spinlock_t Lock;
}
QX_MEM_WORKER;

static QX_MEM_WORKER sg_MemWorker = {
        .MainModId = QX_UTIL_MEM_MODULE_INVALID_ID,
        .Inited = FALSE
    };

int
QXUtil_MemRegister(
    int *MemId,
    char *Name
    )
{
    int ret = QX_SUCCESS;
    int loop = 0;
    BOOL registered = FALSE;
    if (!Name || !MemId || !sg_MemWorker.Inited)
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }
    if (*MemId >= 0 && *MemId < QX_MEM_MODULE_MAX_NUM && 
        sg_MemWorker.Nodes[*MemId].MemModuleId == *MemId && sg_MemWorker.Nodes[*MemId].Registered)
    {
        goto CommonReturn;
    }
    pthread_spin_lock(&sg_MemWorker.Lock);
    {
        for(loop = 0; loop < QX_MEM_MODULE_MAX_NUM; loop ++)
        {
            if (sg_MemWorker.Nodes[loop].Registered)
            {
                continue;
            }
            memset(&sg_MemWorker.Nodes[loop], 0, sizeof(QX_MEM_NODE));
            strcpy(sg_MemWorker.Nodes[loop].MemModuleName, Name);
            pthread_spin_init(&sg_MemWorker.Nodes[loop].MemSpinlock, PTHREAD_PROCESS_PRIVATE);
            sg_MemWorker.Nodes[loop].Registered = TRUE;
            sg_MemWorker.Nodes[loop].MemModuleId = loop;
            *MemId = loop;
            registered = TRUE;
            break;
        }
    }
    pthread_spin_unlock(&sg_MemWorker.Lock);
    if (!registered)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}

int
QXUtil_MemUnRegister(
    int* MemId
    )
{
    int ret = QX_SUCCESS;
    if (sg_MemWorker.Inited && MemId && *MemId >= 0 && *MemId < QX_MEM_MODULE_MAX_NUM && 
        sg_MemWorker.Nodes[*MemId].MemModuleId == *MemId && sg_MemWorker.Nodes[*MemId].Registered)
    {
        pthread_spin_lock(&sg_MemWorker.Lock);
        if (!QXUtil_MemLeakSafetyCheckWithId(*MemId))
        {
            ret = -QX_ERR_MEM_LEAK;
        }
        memset(&sg_MemWorker.Nodes[*MemId], 0, sizeof(QX_MEM_NODE));
        *MemId = QX_UTIL_MEM_MODULE_INVALID_ID;
        pthread_spin_unlock(&sg_MemWorker.Lock);
    }

    return ret;
}

int
QXUtil_MemModuleInit(
    void
    )
{
    if (sg_MemWorker.Inited)
        return QX_SUCCESS;
    
    sg_MemWorker.Inited = TRUE;
    pthread_spin_init(&sg_MemWorker.Lock, PTHREAD_PROCESS_PRIVATE);
    memset(sg_MemWorker.Nodes, 0, sizeof(sg_MemWorker.Nodes));
    return QXUtil_MemRegister(&sg_MemWorker.MainModId, "MemModuleCommon");
}

int
QXUtil_MemModuleExit(
    void
    )
{
    int ret = QX_SUCCESS;
    if (!sg_MemWorker.Inited)
        goto CommonReturn;
    
    sg_MemWorker.Inited = FALSE;
    ret = QXUtil_MemUnRegister(&sg_MemWorker.MainModId);
    pthread_spin_destroy(&sg_MemWorker.Lock);

CommonReturn:
    return ret;
}

int
QXUtil_MemModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = QX_SUCCESS;
    int len = 0;
    int loop = 0;

    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, "<%s:", QXUtil_ModuleNameByEnum(QX_UTIL_MODULES_ENUM_MEM));
    if (len < 0 || len >= BuffMaxLen - *Offset)
    {
        ret = -QX_ENOMEM;
        LogErr("Too long Msg!");
        goto CommonReturn;
    }
    else
    {
        *Offset += len;
    }

    for(loop = 0; loop < QX_MEM_MODULE_MAX_NUM; loop ++)
    {
        if (!sg_MemWorker.Nodes[loop].Registered)
        {
            continue;
        }
        len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, 
                "[MemId=%u, MemName<%s>, MemAlloced=%"PRIu64", MemFreed=%"PRIu64", MemInusing=%"PRIu64"]",
                sg_MemWorker.Nodes[loop].MemModuleId, sg_MemWorker.Nodes[loop].MemModuleName, 
                sg_MemWorker.Nodes[loop].MemBytesAlloced, sg_MemWorker.Nodes[loop].MemBytesFreed,
                sg_MemWorker.Nodes[loop].MemBytesAlloced - sg_MemWorker.Nodes[loop].MemBytesFreed);
        if (len < 0 || len >= BuffMaxLen - *Offset)
        {
            ret = -QX_ENOMEM;
            LogErr("Too long Msg!");
            goto CommonReturn;
        }
        else
        {
            *Offset += len;
        }
    }
    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, ">");
    if (len < 0 || len >= BuffMaxLen - *Offset)
    {
        ret = -QX_ENOMEM;
        LogErr("Too long Msg!");
        goto CommonReturn;
    }
    else
    {
        *Offset += len;
    }
CommonReturn:
    return ret;
}

void*
QXUtil_MemCalloc(
    int MemId,
    size_t Size
    )
{
    void *ret = NULL;
    if (MemId < 0 || MemId > QX_MEM_MODULE_MAX_NUM - 1 || !sg_MemWorker.Nodes[MemId].Registered || Size > 0x7fffffff)
    {
        return NULL;
    }
    ret = calloc(Size + sizeof(QX_MEM_PREFIX), 1);
    if (ret)
    {
        ((QX_MEM_PREFIX*)ret)->MemModuleId = MemId;
        ((QX_MEM_PREFIX*)ret)->Size = Size;
        ((QX_MEM_PREFIX*)ret)->Freed = FALSE;
        pthread_spin_lock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
        sg_MemWorker.Nodes[MemId].MemBytesAlloced += Size;
        pthread_spin_unlock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
    }
    ret = (void*)(((uint8_t*)ret) + sizeof(QX_MEM_PREFIX));
    return ret;
}

void
QXUtil_MemFree(
    int MemId,
    void* Ptr
    )
{
    uint32_t size = 0;
    if (Ptr && MemId >= 0 && MemId < QX_MEM_MODULE_MAX_NUM && sg_MemWorker.Nodes[MemId].Registered)
    {
        Ptr = (void*)(((uint8_t*)Ptr) - sizeof(QX_MEM_PREFIX));
        size = ((QX_MEM_PREFIX*)Ptr)->Size;
        ((QX_MEM_PREFIX*)Ptr)->Freed = TRUE;
        free(Ptr);
        pthread_spin_lock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
        sg_MemWorker.Nodes[MemId].MemBytesFreed += size;
        pthread_spin_unlock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
    }
}

inline void*
QXUtil_QXCalloc(
    size_t Size
    )
{
    void* Ptr = QXUtil_MemCalloc(sg_MemWorker.MainModId, Size);
#ifdef DEBUG
    LogInfo("Size %zu Ptr %p", Size, Ptr);
#endif
    return Ptr;
}

inline void
QXUtil_QXFree(
    void* Ptr
    )
{
#ifdef DEBUG
    LogInfo("free %p", Ptr);
#endif
    return QXUtil_MemFree(sg_MemWorker.MainModId, Ptr);
}

BOOL
QXUtil_MemLeakSafetyCheckWithId(
    int MemId
    )
{
    BOOL ret = TRUE;
    if (!sg_MemWorker.Inited || 
        !(MemId >= 0 && MemId < QX_MEM_MODULE_MAX_NUM && sg_MemWorker.Nodes[MemId].Registered))
    {
        goto CommonReturn;
    }
    pthread_spin_lock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
    {
        LogInfo("%s:alloced:%"PRIu64" freed:%"PRIu64"", sg_MemWorker.Nodes[MemId].MemModuleName, 
            sg_MemWorker.Nodes[MemId].MemBytesAlloced, sg_MemWorker.Nodes[MemId].MemBytesFreed);
        ret = sg_MemWorker.Nodes[MemId].MemBytesFreed == sg_MemWorker.Nodes[MemId].MemBytesAlloced;
    }
    pthread_spin_unlock(&sg_MemWorker.Nodes[MemId].MemSpinlock);
    
CommonReturn:
    return ret;
}

BOOL
QXUtil_MemLeakSafetyCheck(
    void
    )
{
    return QXUtil_MemLeakSafetyCheckWithId(sg_MemWorker.MainModId);
}

