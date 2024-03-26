#include "UnitTest.h"
#include "QXUtilsThreadPool.h"
#include "QXUtilsCommonUtil.h"
#include "QXUtilsMem.h"
#include "QXUtilsLogIO.h"

#define UT_TPOOL_TEST_VAL                                           132
#define UT_TPOOL_TEST_TIMEOUT_VAL                                   133
#define UT_TPOOL_MAX_CPU_USAGE                                      0.1     // 10%

typedef struct _UT_TPOOL_TASK_ARG
{
    int Value;
    void* Ptr;
}
UT_TPOOL_TASK_ARG;

static BOOL sg_UT_TPool_TaskErrHapped = FALSE;

static int
_UT_TPool_PreInit(
    void
    )
{
    return QXUtil_MemModuleInit();
}

static int
_UT_TPool_FinExit(
    void
    )
{
    return QXUtil_MemModuleExit();
}

static void
_UT_TPool_AddTaskCb(
    void* Arg
    )
{
    UT_TPOOL_TASK_ARG *taskArg = (UT_TPOOL_TASK_ARG *)Arg;

    if ((taskArg->Value != UT_TPOOL_TEST_VAL && taskArg->Value != UT_TPOOL_TEST_TIMEOUT_VAL) || taskArg->Ptr != Arg)
    {
        sg_UT_TPool_TaskErrHapped = TRUE;
        UTLog("Invalid arg, value:%d arg:%p taskarg:%p\n", taskArg->Value, taskArg, taskArg->Ptr);
    }
    if (taskArg->Value == UT_TPOOL_TEST_TIMEOUT_VAL)
    {
        UTLog("Waiting ...\n");
        usleep(1500 * 1000);
    }
    QXUtil_QXFree(taskArg);
    UTLog("_TPool_AddTaskCb success\n");
}

static int
_UT_TPool_InitExit(
    void
    )
{
    int ret = QX_SUCCESS;
    QX_UTIL_TPOOL_MODULE_INIT_ARG initArg = {.ThreadPoolSize = 256, .Timeout = 5, .TaskListMaxLength = 1024};

    QXUtil_LogSetLevel(1);
    
    ret = QXUtil_TPoolModuleInit(&initArg);
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

CommonReturn:
    if (QXUtil_TPoolModuleExit())
    {
        ret = -QX_EINVAL;
    }
    if (!QXUtil_MemLeakSafetyCheck())
    {
        ret = -QX_EINVAL;
    }
    QXUtil_LogSetLevel(0);
    return ret;
}

static int
_UT_TPool_ForwardT(
    void
    )
{
    int ret = QX_SUCCESS;
    UT_TPOOL_TASK_ARG *taskArg = NULL;
    double cpuUsage = 0;
    QX_UTIL_TPOOL_MODULE_INIT_ARG initArg = {.ThreadPoolSize = 3, .Timeout = 5, .TaskListMaxLength = 1024};
    
QX_UTIL_GET_CPU_USAGE_START
{
    ret = QXUtil_TPoolModuleInit(&initArg);
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

    taskArg = (UT_TPOOL_TASK_ARG*)QXUtil_QXCalloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = QXUtil_TPoolAddTask(_UT_TPool_AddTaskCb, (void*)taskArg);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }

    taskArg = (UT_TPOOL_TASK_ARG*)QXUtil_QXCalloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = QXUtil_TPoolAddTaskAndWait(_UT_TPool_AddTaskCb, (void*)taskArg, 5);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }
    
    taskArg = (UT_TPOOL_TASK_ARG*)QXUtil_QXCalloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = QXUtil_TPoolAddTask(_UT_TPool_AddTaskCb, (void*)taskArg);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }
    
    taskArg = (UT_TPOOL_TASK_ARG*)QXUtil_QXCalloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_TIMEOUT_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = QXUtil_TPoolAddTaskAndWait(_UT_TPool_AddTaskCb, (void*)taskArg, 1);
    if (ret != -QX_ETIMEDOUT)
    {
        UTLog("wait fail, ret %d\n", ret);
        ret = -QX_EIO;
        goto CommonReturn;
    }
    else
    {
        ret = QX_SUCCESS;
    }
    sleep(1);
    usleep(100 * 100);
}
QX_UTIL_GET_CPU_USAGE_END(cpuUsage);
    UTLog("Cpu usage %lf\n", cpuUsage);

    if (cpuUsage > UT_TPOOL_MAX_CPU_USAGE)
    {
        ret = -QX_E2BIG;
    }
    
CommonReturn:
    if (QXUtil_TPoolModuleExit())
    {
        ret = -QX_EINVAL;
    }
    if (!QXUtil_MemLeakSafetyCheck())
    {
        ret = -QX_EINVAL;
    }
    return ret;
}

int main()
{
    assert(QX_SUCCESS == _UT_TPool_PreInit());
    assert(QX_SUCCESS == _UT_TPool_InitExit());
    assert(QX_SUCCESS == _UT_TPool_FinExit());
    
    assert(QX_SUCCESS == _UT_TPool_PreInit());
    assert(QX_SUCCESS == _UT_TPool_ForwardT());
    assert(QX_SUCCESS == _UT_TPool_FinExit());
    
    assert(FALSE == sg_UT_TPool_TaskErrHapped);

    return 0;
}
