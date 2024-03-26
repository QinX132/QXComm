#include "QXUtilsModuleHealth.h"
#include "QXUtilsList.h"

#define QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL                          300 //s

MODULE_HEALTH_REPORT_REGISTER sg_ModuleReprt[QX_UTIL_MODULES_ENUM_MAX] = 
{
    [QX_UTIL_MODULES_ENUM_LOG]       =   {QXUtil_LogModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_MSG]       =   {QXUtil_MsgModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_TPOOL]     =   {QXUtil_TPoolModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_CMDLINE]   =   {QXUtil_CmdLineModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_MHEALTH]   =   {QXUtil_HealthModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_MEM]       =   {QXUtil_MemModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [QX_UTIL_MODULES_ENUM_TIMER]     =   {QXUtil_TimerModuleCollectStat, QX_MODULE_HEALTH_DEFAULT_TIME_INTERVAL}
};

typedef struct _QX_HEALTH_MONITOR_LIST_NODE
{
    struct event* Event;
    char Name[QX_UTIL_HEALTH_MONITOR_NAME_MAX_LEN];
    int32_t IntervalS;
    QX_LIST_NODE List;
}
QX_HEALTH_MONITOR_LIST_NODE;

typedef struct _QX_HEALTH_MONITOR
{
    pthread_t ThreadId;
    struct event_base* EventBase;
    BOOL IsRunning;
    QX_LIST_NODE EventList;     // MY_HEALTH_MONITOR_LIST_NODE
    int32_t EventListLen;
    pthread_spinlock_t Lock;
}
QX_HEALTH_MONITOR;

static QX_HEALTH_MONITOR sg_HealthWorker = {.IsRunning = FALSE};
static int32_t sg_HealthModId = QX_UTIL_MEM_MODULE_INVALID_ID;

static void*
_HealthCalloc(
    size_t Size
    )
{
    return QXUtil_MemCalloc(sg_HealthModId, Size);
}

static void
_HealthFree(
    void* Ptr
    )
{
    return QXUtil_MemFree(sg_HealthModId, Ptr);
}

#define QX_HEALTH_MONITOR_KEEPALIVE_INTERVAL                1 // s
static void
_HealthMonitorKeepalive(
    evutil_socket_t Fd,
    short Event,
    void *Arg
    )
{
    UNUSED(Fd);
    UNUSED(Event);
    UNUSED(Arg);
    return ;
}

static void
_HealthModuleStatCommonTemplate(
    evutil_socket_t Fd,
    short Event,
    void *Arg
    )
{
    char logBuff[QX_BUFF_1024 * QX_BUFF_1024] = {0};
    int offset = 0;
    
    UNUSED(Arg);
    UNUSED(Fd);
    UNUSED(Event);

    StatReportCB cb = (StatReportCB)Arg;
    
    if (cb(logBuff, sizeof(logBuff), &offset) == 0 && strlen(logBuff))
    {
        LogDbg("%s", logBuff);
    }
    return ;
}

static void*
_HealthModuleEntry(
    void* Arg
    )
{
    struct timeval tv;
    int loop = 0;
    QX_HEALTH_MONITOR_LIST_NODE *node = NULL;
    
    UNUSED(Arg);

    sg_HealthWorker.EventBase = event_base_new();
    if(!sg_HealthWorker.EventBase)
    {
        LogErr("New event base failed!\n");
        goto CommonReturn;
    }
    // Set event base to be externally referencable and thread-safe
    if (evthread_make_base_notifiable(sg_HealthWorker.EventBase) < 0)
    {
        goto CommonReturn;
    }
    // keepalive
    node = (QX_HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(QX_HEALTH_MONITOR_LIST_NODE));
    if (!node)
    {
        goto CommonReturn;
    }
    node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
    if (!node->Event)
    {
        goto CommonReturn;
    }
    tv.tv_sec = QX_HEALTH_MONITOR_KEEPALIVE_INTERVAL;
    tv.tv_usec = 0;
    snprintf(node->Name, sizeof(node->Name), "%s", "Keepalive");
    node->IntervalS = QX_HEALTH_MONITOR_KEEPALIVE_INTERVAL;
    event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_READ | EV_PERSIST, _HealthMonitorKeepalive, NULL);
    event_add(node->Event, &tv);
    event_active(node->Event, EV_READ, 0);
    QX_LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen ++;
    node = NULL;
    
    for(loop = 0; loop < QX_UTIL_MODULES_ENUM_MAX; loop ++)
    {
        if (sg_ModuleReprt[loop].Cb && sg_ModuleReprt[loop].Interval > 0)
        {
            node = (QX_HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(QX_HEALTH_MONITOR_LIST_NODE));
            if (!node)
            {
                goto CommonReturn;
            }
            node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
            if (!node->Event)
            {
                goto CommonReturn;
            }
            snprintf(node->Name, sizeof(node->Name), "%s", QXUtil_ModuleNameByEnum(loop));
            node->IntervalS = sg_ModuleReprt[loop].Interval;
            tv.tv_sec = sg_ModuleReprt[loop].Interval;
            tv.tv_usec = 0;
            event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_PERSIST, _HealthModuleStatCommonTemplate, (void*)sg_ModuleReprt[loop].Cb);
            event_add(node->Event, &tv);
            QX_LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
            sg_HealthWorker.EventListLen ++;
            node = NULL;
        }
    }

    sg_HealthWorker.IsRunning = TRUE;
    event_base_dispatch(sg_HealthWorker.EventBase);
    
CommonReturn:
    sg_HealthWorker.IsRunning = FALSE;
    if (node)
    {
        _HealthFree(node);
    }
    if (sg_HealthWorker.EventBase)
    {
        event_base_free(sg_HealthWorker.EventBase);
        sg_HealthWorker.EventBase = NULL;
    }
    pthread_exit(NULL);
}

int
QXUtil_HealthMonitorAdd(
    StatReportCB Cb,
    const char* Name,
    int TimeIntervalS
    )
{
    int ret = QX_SUCCESS;
    QX_HEALTH_MONITOR_LIST_NODE *node = NULL;
    struct timeval tv;
    
    node = (QX_HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(QX_HEALTH_MONITOR_LIST_NODE));
    if (!node)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
    if (!node->Event)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    tv.tv_sec = TimeIntervalS;
    tv.tv_usec = 0;
    if (Name)
    {
        snprintf(node->Name, sizeof(node->Name), "%s", Name);
    }
    node->IntervalS = TimeIntervalS;
    pthread_spin_lock(&sg_HealthWorker.Lock);
    event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_PERSIST, _HealthModuleStatCommonTemplate, (void*)Cb);
    event_add(node->Event, &tv);
    QX_LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen ++;
    pthread_spin_unlock(&sg_HealthWorker.Lock);

CommonReturn:
    if (ret && node)
    {
        _HealthFree(node);
    }
    return ret;
}

int
QXUtil_HealthModuleExit(
    void
    )
{
    int ret = QX_SUCCESS;
    QX_HEALTH_MONITOR_LIST_NODE *tmp = NULL, *loop = NULL;
    
    if (sg_HealthWorker.IsRunning)
    {
        pthread_spin_lock(&sg_HealthWorker.Lock);
        sg_HealthWorker.EventListLen = 0;
        if (!QX_LIST_IS_EMPTY(&sg_HealthWorker.EventList))
        {
            QX_LIST_FOR_EACH(&sg_HealthWorker.EventList, loop, tmp, QX_HEALTH_MONITOR_LIST_NODE, List)
            {
                QX_LIST_DEL_NODE(&loop->List);
                event_del(loop->Event);
                //event_free(loop->Event);  // no need to free because we use event assign
                _HealthFree(loop->Event);
                _HealthFree(loop);
                loop = NULL;
            }
        }
        pthread_spin_unlock(&sg_HealthWorker.Lock);
        event_base_loopexit(sg_HealthWorker.EventBase, NULL);
        pthread_join(sg_HealthWorker.ThreadId, NULL);
        pthread_spin_destroy(&sg_HealthWorker.Lock);
        ret = QXUtil_MemUnRegister(&sg_HealthModId);
    }

    return ret;
}

int 
QXUtil_HealthModuleInit(
    QX_UTIL_HEALTH_MODULE_INIT_ARG *InitArg
    )
{
    int ret = QX_SUCCESS;
    int sleepIntervalUs = 10;
    int waitTimeUs = sleepIntervalUs * 1000; // 10 ms

    if (sg_HealthWorker.IsRunning)
    {
        goto CommonReturn;
    }
    ret = QXUtil_MemRegister(&sg_HealthModId, "Health");
    if (ret)
    {
        LogErr("Register mem failed!\n");
        goto CommonReturn;
    }

    if (InitArg)
    {
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_LOG].Interval = InitArg->LogHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_MSG].Interval = InitArg->MsgHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_TPOOL].Interval = InitArg->TPoolHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_CMDLINE].Interval = InitArg->CmdLineHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_MHEALTH].Interval = InitArg->MHHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_MEM].Interval = InitArg->MemHealthIntervalS;
        sg_ModuleReprt[QX_UTIL_MODULES_ENUM_TIMER].Interval = InitArg->TimerHealthIntervalS;
    }
    
    pthread_spin_init(&sg_HealthWorker.Lock, PTHREAD_PROCESS_PRIVATE);
    QX_LIST_NODE_INIT(&sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen = 0;
    ret = pthread_create(&sg_HealthWorker.ThreadId, NULL, _HealthModuleEntry, NULL);
    if (ret)
    {
        goto CommonReturn;
    }

    while(!sg_HealthWorker.IsRunning && waitTimeUs >= 0)
    {
        usleep(sleepIntervalUs);
        waitTimeUs -= sleepIntervalUs;
    }

CommonReturn:
    return ret;
}

int
QXUtil_HealthModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = QX_SUCCESS;
    QX_HEALTH_MONITOR_LIST_NODE *tmp = NULL, *loop = NULL;
    int len = 0;

    len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
            "<%s:(ListLength:%d)", QXUtil_ModuleNameByEnum(QX_UTIL_MODULES_ENUM_MHEALTH), sg_HealthWorker.EventListLen);
    if (!QX_LIST_IS_EMPTY(&sg_HealthWorker.EventList))
    {
        QX_LIST_FOR_EACH(&sg_HealthWorker.EventList, loop, tmp, QX_HEALTH_MONITOR_LIST_NODE, List)
        {
            len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
                "[EventName:%s, EventInterval:%d]", loop->Name, loop->IntervalS);
            if (len < 0 || len >= BuffMaxLen - *Offset - len)
            {
                ret = -QX_ENOMEM;
                LogErr("Too long Msg!");
                goto CommonReturn;
            }
        }
        
    }
    
    len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, ">");
    
CommonReturn:
    *Offset += len;
    return ret;
}

static const char* sg_ModulesName[QX_UTIL_MODULES_ENUM_MAX] = 
{
    [QX_UTIL_MODULES_ENUM_LOG]       =   "Log",
    [QX_UTIL_MODULES_ENUM_MSG]       =   "Msg",
    [QX_UTIL_MODULES_ENUM_TPOOL]     =   "TPool",
    [QX_UTIL_MODULES_ENUM_CMDLINE]   =   "CmdLine",
    [QX_UTIL_MODULES_ENUM_MHEALTH]   =   "MHealth",
    [QX_UTIL_MODULES_ENUM_MEM]       =   "Mem",
    [QX_UTIL_MODULES_ENUM_TIMER]     =   "Timer"
};

const char*
QXUtil_ModuleNameByEnum(
    int Module
    )
{
    return (Module >= 0 && Module < (int)QX_UTIL_MODULES_ENUM_MAX) ? sg_ModulesName[Module] : NULL;
}

