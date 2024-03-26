#include "QXUtilsThreadPool.h"
#include "QXUtilsModuleHealth.h"
#include "QXUtilsLogIO.h"
#include "QXUtilsList.h"
#include "QXUtilsCommonUtil.h"

#define QX_TASK_TIME_OUT_DEFAULT_VAL                            5 //seconds
#define THREAD_POOL_SIZE                                        5
#define THREAD_POOL_MAX_TASK_LENGTH                             1024

typedef enum _QX_TPOOL_TASK_STATUS
{
    QX_TPOOL_TASK_STATUS_UNSPEC,
    QX_TPOOL_TASK_STATUS_INIT,
    QX_TPOOL_TASK_STATUS_WORKING,
    QX_TPOOL_TASK_STATUS_TIMEOUT
}
QX_TPOOL_TASK_STATUS;

typedef struct _QX_TPOOL_TASK
{
    void (*TaskFunc)(void*);
    void* TaskArg;
    BOOL HasTimeOut;
    pthread_mutex_t *TaskLock;
    pthread_cond_t *TaskCond;
    QX_TPOOL_TASK_STATUS TaskStat;
    QX_LIST_NODE List;
}
QX_TPOOL_TASK;

typedef struct _QX_THREAD_POOL_STATS
{
    int TaskAdded;
    int TaskSucceed;
    int TaskFailed;
}
QX_THREAD_POOL_STATS;

typedef struct _QX_THREAD_POOL{
    pthread_mutex_t Lock;
    pthread_cond_t Cond;
    pthread_t *Threads;
    volatile int CurrentThreadNum;
    QX_LIST_NODE TaskListHead;      // MY_TEST_THREAD_TASK
    int TaskListLength;
    int TaskMaxLength;
    int TaskDefaultTimeout;
    volatile BOOL Exit;
}
QX_THREAD_POOL;

//__thread 
QX_THREAD_POOL* sg_ThreadPool = NULL;
static QX_THREAD_POOL_STATS sg_ThreadPoolStats = {.TaskAdded = 0, .TaskSucceed = 0, .TaskFailed = 0};
//__thread 
BOOL sg_TPoolModuleInited = FALSE;
static int sg_TPoolMemId = QX_UTIL_MEM_MODULE_INVALID_ID;

static void*
_TPoolCalloc(
    size_t Size
    )
{
    return QXUtil_MemCalloc(sg_TPoolMemId, Size);
}

static void
_TPoolFree(
    void* Ptr
    )
{
    return QXUtil_MemFree(sg_TPoolMemId, Ptr);
}

static void* 
_TPoolProcFn(
    void* arg
    )
{
    QX_THREAD_POOL* threadPool = (QX_THREAD_POOL*)arg;
    QX_TPOOL_TASK* loop = NULL, *tmp = NULL;
    QX_LIST_NODE listHeadTmp;
    BOOL hasTask = FALSE;
    LogInfo("Thread worker %lu entering...", syscall(SYS_gettid));
    
    while (!threadPool->Exit) 
    {
        hasTask = FALSE;
        pthread_mutex_lock(&threadPool->Lock);
        QX_UATOMIC_INC(&threadPool->CurrentThreadNum);
        do{
            if (threadPool->Exit)
            {
                pthread_mutex_unlock(&threadPool->Lock);
                goto CommonReturn;
            }
            if (!QX_LIST_IS_EMPTY(&threadPool->TaskListHead))
            {
                QX_LIST_HEAD_COPY(&listHeadTmp, &threadPool->TaskListHead);
                QX_LIST_NODE_INIT(&threadPool->TaskListHead);
                threadPool->TaskListLength = 0;
                hasTask = TRUE;
                break;
            }
        }while(pthread_cond_wait(&threadPool->Cond, &threadPool->Lock) == 0);
        QX_UATOMIC_DEC(&threadPool->CurrentThreadNum);
        pthread_mutex_unlock(&threadPool->Lock);

        if (!hasTask)
        {
            continue;
        }
        
        
        QX_LIST_FOR_EACH(&listHeadTmp, loop, tmp, QX_TPOOL_TASK, List)
        {
            if (loop && loop->TaskFunc)
            {
                if (loop->HasTimeOut && loop->TaskStat == QX_TPOOL_TASK_STATUS_TIMEOUT)
                {
                    pthread_mutex_destroy(loop->TaskLock);
                    pthread_cond_destroy(loop->TaskCond);
                    _TPoolFree(loop->TaskLock);
                    _TPoolFree(loop->TaskCond);
                }
                else
                {
                    loop->TaskFunc(loop->TaskArg);
                    if (loop->HasTimeOut)
                    {
                        if (loop->TaskStat == QX_TPOOL_TASK_STATUS_TIMEOUT)
                        {
                            pthread_mutex_destroy(loop->TaskLock);
                            pthread_cond_destroy(loop->TaskCond);
                            _TPoolFree(loop->TaskLock);
                            _TPoolFree(loop->TaskCond);
                        }
                        else
                        {
                            pthread_mutex_lock(loop->TaskLock);
                            pthread_cond_signal(loop->TaskCond);
                            pthread_mutex_unlock(loop->TaskLock);
                        }
                    }
                    QX_UATOMIC_INC(&sg_ThreadPoolStats.TaskSucceed);
                }
                QX_LIST_DEL_NODE(&loop->List);
                _TPoolFree(loop);
                loop = NULL;
            }
        }
    }

CommonReturn:
    QX_UATOMIC_DEC(&threadPool->CurrentThreadNum);
    LogInfo("Thread worker %lu exit.", syscall(SYS_gettid));
    pthread_exit(NULL);
}

int
QXUtil_TPoolModuleInit(
    QX_UTIL_TPOOL_MODULE_INIT_ARG *InitArg
    )
{
    int loop = 0;
    int ret = QX_SUCCESS;
    pthread_attr_t attr;
    int sleepIntervalMs = 1;
    int waitTimeMs = sleepIntervalMs * 10; // 10 ms

    if (sg_TPoolModuleInited)
    {
        goto CommonReturn;
    }

    if (!InitArg || InitArg->ThreadPoolSize <= 0)
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }

    ret = QXUtil_MemRegister(&sg_TPoolMemId, "TPool");
    if (ret)
    {
        LogErr("Mem register failed! ret %d", ret);
        goto CommonReturn;
    }
    
    sg_ThreadPool = (QX_THREAD_POOL*)_TPoolCalloc(sizeof(QX_THREAD_POOL));
    if (!sg_ThreadPool)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    
    sg_ThreadPool->TaskMaxLength = InitArg->TaskListMaxLength ? 
        InitArg->TaskListMaxLength : THREAD_POOL_MAX_TASK_LENGTH;
    
    pthread_mutex_init(&sg_ThreadPool->Lock, NULL);
    pthread_cond_init(&sg_ThreadPool->Cond, NULL);
    sg_ThreadPool->Exit = FALSE;
    sg_ThreadPool->CurrentThreadNum = 0;
    QX_LIST_NODE_INIT(&sg_ThreadPool->TaskListHead);
    sg_ThreadPool->TaskListLength = 0;
    
    ret = pthread_attr_init(&attr);
    assert(!ret);
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(!ret);
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        sg_ThreadPool->Threads = (pthread_t*)_TPoolCalloc(sizeof(pthread_t) * InitArg->ThreadPoolSize);
        for (loop = 0; loop < InitArg->ThreadPoolSize; loop ++)
        {
            ret = pthread_create(&(sg_ThreadPool->Threads[loop]), &attr, _TPoolProcFn, (void*)sg_ThreadPool);
            assert(!ret);
        }
        ret = pthread_attr_destroy(&attr);
        assert(!ret);
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);

    sg_ThreadPool->TaskDefaultTimeout = InitArg->Timeout ? InitArg->Timeout : QX_TASK_TIME_OUT_DEFAULT_VAL;
    sg_TPoolModuleInited = TRUE;

    // to make sure workers are ready
    while(waitTimeMs >= 0)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        if (sg_ThreadPool->CurrentThreadNum >= InitArg->ThreadPoolSize)
        {
            pthread_mutex_unlock(&sg_ThreadPool->Lock);
            break;
        }
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
        usleep(sleepIntervalMs * 1000);
        waitTimeMs -= sleepIntervalMs;
    };
    
CommonReturn:
    return ret;
}

int
QXUtil_TPoolModuleExit(
    void
    )
{
    int loop = 0, ret = QX_SUCCESS;

    if (sg_TPoolModuleInited)
    {
        sg_ThreadPool->Exit = TRUE;
        pthread_mutex_lock(&(sg_ThreadPool->Lock));
        pthread_cond_broadcast(&(sg_ThreadPool->Cond));
        pthread_mutex_unlock(&(sg_ThreadPool->Lock));

        for (loop = 0; loop < sg_ThreadPool->CurrentThreadNum; loop++) {
            pthread_cond_broadcast(&(sg_ThreadPool->Cond));
        }

        pthread_mutex_destroy(&(sg_ThreadPool->Lock));
        pthread_cond_destroy(&(sg_ThreadPool->Cond));
        _TPoolFree(sg_ThreadPool->Threads);
        _TPoolFree(sg_ThreadPool);
        sg_ThreadPool = NULL;
        sg_TPoolModuleInited = FALSE;
        ret = QXUtil_MemUnRegister(&sg_TPoolMemId);
    }

    return ret;
}

// async api, TaskArg is a in arg
int
QXUtil_TPoolAddTask(
    void (*TaskFunc)(void*),
    __in void* TaskArg
    )
{
    int ret = QX_SUCCESS;
    QX_TPOOL_TASK *node = NULL;
    
    if (!sg_TPoolModuleInited)
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        if (sg_ThreadPool->TaskListLength >= sg_ThreadPool->TaskMaxLength)
        {
            ret = -QX_EBUSY;
            LogErr("%d task in schedule!", sg_ThreadPool->TaskListLength);
            pthread_cond_signal(&sg_ThreadPool->Cond);
            pthread_mutex_unlock(&sg_ThreadPool->Lock);
            goto CommonReturn;
        }
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);
    
    node = (QX_TPOOL_TASK*)_TPoolCalloc(sizeof(QX_TPOOL_TASK));
    if (!node) 
    {
        ret = -QX_ENOMEM;
        LogErr("Apply mem failed!");
        goto CommonReturn;
    }
    node->HasTimeOut = FALSE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    QX_LIST_NODE_INIT(&node->List);
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        QX_LIST_ADD_TAIL(&node->List, &sg_ThreadPool->TaskListHead);
        sg_ThreadPool->TaskListLength ++;
        pthread_cond_signal(&sg_ThreadPool->Cond);
        QX_UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
        LogInfo("Add task into tail success.");
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);
    
CommonReturn:
    return ret;
}

// sync api, you can care about TaskArg inout
// when TimeoutSec > 0, use it; otherwise use sg_TPoolTaskTimeout
int
QXUtil_TPoolAddTaskAndWait(
    void (*TaskFunc)(void*),
    __inout void* TaskArg,
    int32_t TimeoutSec
    )
{
    int ret = QX_SUCCESS;
    struct timespec ts = {0, 0};
    BOOL taskAdded = FALSE;
    pthread_mutex_t *taskLock;
    pthread_cond_t *taskCond;
    pthread_condattr_t taskAttr;
    BOOL taskInited = FALSE;
    QX_TPOOL_TASK *node = NULL;
    
    if (!sg_TPoolModuleInited || (sg_ThreadPool->TaskDefaultTimeout <= 0 && TimeoutSec <= 0))
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }
    taskLock = (pthread_mutex_t*)_TPoolCalloc(sizeof(pthread_mutex_t));
    taskCond = (pthread_cond_t*)_TPoolCalloc(sizeof(pthread_cond_t));
    node = (QX_TPOOL_TASK*)_TPoolCalloc(sizeof(QX_TPOOL_TASK));
    if (!taskLock || !taskCond || !node)
    {
        ret = -QX_ENOMEM;
        LogErr("Apply mem failed!");
        goto CommonReturn;
    }
    
    pthread_condattr_init(&taskAttr);
    pthread_condattr_setclock(&taskAttr, CLOCK_MONOTONIC);
    pthread_mutex_init(taskLock, NULL);
    pthread_cond_init(taskCond, &taskAttr);
    pthread_condattr_destroy(&taskAttr);
    taskInited = TRUE;
    
    node->HasTimeOut = TRUE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    node->TaskCond = taskCond;
    node->TaskLock = taskLock;
    node->TaskStat = QX_TPOOL_TASK_STATUS_INIT;
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == QX_SUCCESS)
        {
            QX_LIST_ADD_TAIL(&node->List, &sg_ThreadPool->TaskListHead);
            sg_ThreadPool->TaskListLength ++;
            taskAdded = TRUE;
            pthread_mutex_lock(taskLock);
            pthread_cond_signal(&sg_ThreadPool->Cond);
            QX_UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
        }
        else
        {
            ret = -QX_EIO;
            LogErr("Get time failed!");
        }
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);

    if (taskAdded)
    {
        if (TimeoutSec > 0)
        {
            ts.tv_sec += TimeoutSec;
            LogInfo("Add with timeout %d", TimeoutSec);
        }
        else
        {
            ts.tv_sec += sg_ThreadPool->TaskDefaultTimeout;
            LogInfo("Add with timeout %d", sg_ThreadPool->TaskDefaultTimeout);
        }
        ret = -pthread_cond_timedwait(taskCond, taskLock, &ts);
    }
    if (-QX_ETIMEDOUT == ret)
    {
        LogErr("Task wait timeout!");
        node->TaskStat = QX_TPOOL_TASK_STATUS_TIMEOUT;
    }
    pthread_mutex_unlock(taskLock);
    
CommonReturn:
    ret == QX_SUCCESS ? UNUSED(ret) : QX_UATOMIC_INC(&sg_ThreadPoolStats.TaskFailed);
    if (taskInited)
    {
        if (!taskAdded || ret != -QX_ETIMEDOUT)
        {
            pthread_mutex_destroy(taskLock);
            pthread_cond_destroy(taskCond);
            _TPoolFree(taskLock);
            _TPoolFree(taskCond);
        }
    }
    return ret;
}

int
QXUtil_TPoolModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = QX_SUCCESS;
    int len = 0;
    
    if (!sg_TPoolModuleInited)
    {
        // protect sg_ThreadPool
        goto CommonReturn;
    }
    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, 
        "<%s:[TaskAdded=%d, TaskSucceed=%d, TaskFailed=%d, CurrentThreadNum=%d, TaskListLength=%d]>",
            QXUtil_ModuleNameByEnum(QX_UTIL_MODULES_ENUM_TPOOL), 
            sg_ThreadPoolStats.TaskAdded, sg_ThreadPoolStats.TaskSucceed, sg_ThreadPoolStats.TaskFailed,
            sg_ThreadPool->CurrentThreadNum, sg_ThreadPool->TaskListLength);
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

void
QXUtil_TPoolSetTimeout(
    uint32_t Timeout
    )
{
    if (sg_TPoolModuleInited && sg_ThreadPool)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        sg_ThreadPool->TaskDefaultTimeout = Timeout;
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
    }
}

void
QXUtil_TPoolSetMaxQueueLength(
    int32_t QueueLen
    )
{
    if (sg_TPoolModuleInited && sg_ThreadPool)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        sg_ThreadPool->TaskMaxLength = QueueLen;
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
    }
}

