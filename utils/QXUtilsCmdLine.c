#define _GNU_SOURCE

#include <sys/un.h>

#include "QXUtilsLogIO.h"
#include "QXUtilsCmdLine.h"
#include "QXUtilsCommonUtil.h"
#include "QXUtilsModuleHealth.h"

#define CMDLINE_USAGE_OPT_PRINT_OFFSET                                      36
#define CMDLINE_BUFFER_LEN                                                  (QX_BUFF_1024 * QX_BUFF_1024 * 50)

typedef struct _QX_CMDLINE_CONT
{
    char* Opt;
    char* Help;
    int Argc;
}
QX_CMDLINE_CONT;

typedef struct _QX_CMNLINE_WORKER_STATS
{
    int ExecCnt;
    int ConnectCnt;
}
QX_CMNLINE_WORKER_STATS;

typedef enum _QX_CMDLINE_ROLE
{
    QX_CMDLINE_ROLE_SVR,
    QX_CMDLINE_ROLE_CLT,
    
    QX_CMDLINE_ROLE_UNUSED
}
QX_CMDLINE_ROLE;

typedef struct _QX_CMNLINE_WORKER
{
    QX_CMDLINE_ROLE Role;
    pthread_t *Thread;
    ExitHandle ExitCb;
    BOOL Exit;
    QX_CMNLINE_WORKER_STATS Stat;
}
QX_CMNLINE_WORKER;

typedef struct _QX_CMD_EXTERNAL_NODE
{
    QX_UTIL_CMD_EXTERNAL_CONT Content;
    QX_LIST_NODE List;
}
QX_CMD_EXTERNAL_NODE;

typedef struct _QX_CMD_EXTERN_WORKER
{
    BOOL Inited;
    pthread_spinlock_t Lock;
    QX_LIST_NODE* ExternalCmdHead;       // MY_CMD_EXTERNAL_NODE
}
QX_CMD_EXTERN_WORKER;

static QX_CMNLINE_WORKER sg_CmdLineWorker = {
        .Role = QX_CMDLINE_ROLE_UNUSED, 
        .Thread = NULL,
        .ExitCb = NULL,
        .Exit = TRUE,
        .Stat = { .ExecCnt = 0, .ConnectCnt = 0},
    };

static QX_CMD_EXTERN_WORKER sg_CmdExternalWorker = {.Inited = FALSE};

#define QX_CMDLINE_ARG_LIST                 \
        __QX_CMDLINE_ARG("start", "start this program", 2)  \
        __QX_CMDLINE_ARG("stop", "stop this program", 2)  \
        __QX_CMDLINE_ARG("help", "show this page", 2)  \
        __QX_CMDLINE_ARG("showModuleStat", "show this program's modules stats", 2)  \
        __QX_CMDLINE_ARG("changeTPoolTimeout", "<Timout> (second)", 3)  \
        __QX_CMDLINE_ARG("changeTPoolMaxQueueLength", "<legnth> set tpool max que length", 3)  \
        __QX_CMDLINE_ARG("changeLogLevel", "<Level> (0-info 1-debug 2-warn 3-error)", 3)

typedef enum _QX_CMD_TYPE
{
    QX_CMD_TYPE_START,
    QX_CMD_TYPE_STOP,
    QX_CMD_TYPE_HELP,
    QX_CMD_TYPE_SHOWSTAT,
    QX_CMD_TYPE_CHANGE_TPOOL_TIMEOUT,
    QX_CMD_TYPE_CHANGE_TPOOL_QUEUE_LENGTH,
    QX_CMD_TYPE_CHANGE_LOG_LEVEL,
    
    QX_CMD_TYPE_MAX_UNUSED
}
QX_CMD_TYPE;

static const QX_CMDLINE_CONT sg_CmdLineCont[QX_CMD_TYPE_MAX_UNUSED] = 
{
#undef __QX_CMDLINE_ARG
#define __QX_CMDLINE_ARG(_opt_,_help_, _argc_) \
    {_opt_, _help_, _argc_},
    QX_CMDLINE_ARG_LIST
#undef __QX_CMDLINE_ARG
};

static void
_CmdLineUsage(
    char* RoleName
    )
{
    QX_CMD_EXTERNAL_NODE *loop = NULL, *tmp = NULL;
    
    printf("----------------------------------------------");
    printf("----------------------------------------------\n");
    printf("%10s Usage:\n\n", RoleName ? RoleName : "CmdLine");
#undef __QX_CMDLINE_ARG
#define __QX_CMDLINE_ARG(_opt_,_help_,_argc_) \
    printf("%*s: [%-s]\n", CMDLINE_USAGE_OPT_PRINT_OFFSET, _opt_, _help_);
    QX_CMDLINE_ARG_LIST
#undef __QX_CMDLINE_ARG
    if (sg_CmdExternalWorker.Inited && !QX_LIST_IS_EMPTY(sg_CmdExternalWorker.ExternalCmdHead))
    {
        pthread_spin_lock(&sg_CmdExternalWorker.Lock);
        QX_LIST_FOR_EACH(sg_CmdExternalWorker.ExternalCmdHead, loop, tmp, QX_CMD_EXTERNAL_NODE, List)
        {
            printf("%*s: [%-s]\n", CMDLINE_USAGE_OPT_PRINT_OFFSET, loop->Content.Opt, loop->Content.Help);
        }
        pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
    }
    printf("\n----------------------------------------------");
    printf("----------------------------------------------\n");
}

static int 
_CmdServerInit(
    __inout int *ServerFd,
    __inout int *EpollFd,
    __in char *RoleName
    )
{
    int ret = QX_SUCCESS;
    int epoll_fd = -1;
    struct epoll_event event;
    int serverFd = -1;
    int32_t reuseable = 1; // set port reuseable when fd closed
    int nonBlock = 0;
    char unixDomainPath[QX_BUFF_128] = {0};
    size_t len = 0;
    struct sockaddr_un serverAddr;

    if (!ServerFd || !EpollFd)
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }

    len = snprintf(unixDomainPath + 1, sizeof(unixDomainPath) - 1, "%s.domain", RoleName) + 1;
    /* init server fd */
    // set reuseable
    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    LogInfo("Open serverFd %d", serverFd);
    (void)setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable));
    // set fd nonBlock
    nonBlock = fcntl(serverFd, F_GETFL, 0);
    nonBlock |= O_NONBLOCK;
    fcntl(serverFd, F_SETFL, nonBlock);
    // bind
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    memcpy(serverAddr.sun_path, unixDomainPath, sizeof(serverAddr.sun_path) > len ? len : sizeof(serverAddr.sun_path));
    unlink(unixDomainPath);
    if(bind(serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        ret = -errno;
        LogErr("Bind failed");
        fflush(stdout);
        goto CommonReturn;
    }
    if(listen(serverFd, 5))
    {
        ret = -errno;
        LogErr("Listen failed");
        fflush(stdout);
        goto CommonReturn;
    }

    /* init epool */
    epoll_fd = epoll_create1(0);
    if (0 > epoll_fd)
    {
        ret = -errno;
        LogErr("Create epoll socket failed %d", errno);
        fflush(stdout);
        goto CommonReturn;
    }
    event.events = EPOLLIN; // LT fd
    event.data.fd = serverFd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverFd, &event))
    {
        ret = -errno;
        LogErr("Add to epoll socket failed %d", errno);
        fflush(stdout);
        goto CommonReturn;
    }

CommonReturn:
    if (epoll_fd <= 0 || serverFd <= 0)
    {
        ret = -QX_EIO;
        LogErr("Create fd failed!");
    }
    else
    {
        *EpollFd = epoll_fd;
        *ServerFd = serverFd;
    }
    return ret;
}

static int
_CmdServerHandleMsg(
    int Fd,
    char* Buff
    )
{
    int ret = QX_SUCCESS;
    int len = 0;
    char *retString = NULL;
    QX_CMD_EXTERNAL_NODE *loop = NULL, *tmp = NULL;
    BOOL matchedInExternal = FALSE;

    retString = (char*)malloc(CMDLINE_BUFFER_LEN);
    if (!retString) {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    memset(retString, 0, CMDLINE_BUFFER_LEN);

    QX_UATOMIC_INC(&sg_CmdLineWorker.Stat.ExecCnt);
    
    if (sg_CmdExternalWorker.Inited && !QX_LIST_IS_EMPTY(sg_CmdExternalWorker.ExternalCmdHead))
    {
        pthread_spin_lock(&sg_CmdExternalWorker.Lock);
        QX_LIST_FOR_EACH(sg_CmdExternalWorker.ExternalCmdHead, loop, tmp, QX_CMD_EXTERNAL_NODE, List)
        {
            if (strcasestr(Buff, loop->Content.Opt))
            {
                ret = loop->Content.Cb(Buff + strlen(loop->Content.Opt), strlen(Buff) - strlen(loop->Content.Opt), retString, CMDLINE_BUFFER_LEN);
                matchedInExternal = TRUE;
                break;
            }
        }
        pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
    }
    if (matchedInExternal)
    {
        goto CommonReturn;
    }
    
    if (strcasestr(Buff, sg_CmdLineCont[QX_CMD_TYPE_STOP].Opt))
    {
        sprintf(retString, "Process stopped.");
        len = send(Fd, retString, strlen(retString) + 1, 0);
        if (len <= 0)
        {
            ret = -QX_EIO;
            LogErr("Send CmdLine reply failed!");
        }
        retString[0] = 0;
        if (sg_CmdLineWorker.ExitCb)
        {
            sg_CmdLineWorker.ExitCb();
        }
    }
    else if (strcasestr(Buff, sg_CmdLineCont[QX_CMD_TYPE_SHOWSTAT].Opt))
    {
        int offset = 0;
        int loop = 0;
        retString[offset ++] = '\n';
        for(loop = 0; loop < QX_UTIL_MODULES_ENUM_MAX; loop ++)
        {
            if (sg_ModuleReprt[loop].Cb)
            {
                ret = sg_ModuleReprt[loop].Cb(retString, CMDLINE_BUFFER_LEN, &offset);
                if (ret)
                {
                    LogErr("Get module report failed! ret %d!", ret);
                    break;
                }
                retString[offset ++] = '\n';
            }
        }
        if (ret)
        {
            memset(retString, 0, CMDLINE_BUFFER_LEN);
            offset = strlen("Get module stats failed!");
            strcpy(retString, "Get module stats failed!");
        }
    }
    else if (strcasestr(Buff, sg_CmdLineCont[QX_CMD_TYPE_CHANGE_TPOOL_TIMEOUT].Opt))
    {
        uint32_t timeout = (uint32_t)atoi(strchr(Buff, ' '));
        sprintf(retString, "Set tpool timeout as %u.", timeout);
        QXUtil_TPoolSetTimeout(timeout);
    }
    else if (strcasestr(Buff, sg_CmdLineCont[QX_CMD_TYPE_CHANGE_TPOOL_QUEUE_LENGTH].Opt))
    {
        int32_t queueLength = (int32_t)atoi(strchr(Buff, ' '));
        sprintf(retString, "Set tpool queue length as %u.", queueLength);
        QXUtil_TPoolSetMaxQueueLength(queueLength);
    }
    else if (strcasestr(Buff, sg_CmdLineCont[QX_CMD_TYPE_CHANGE_LOG_LEVEL].Opt))
    {
        uint32_t logLevel = (uint32_t)atoi(strchr(Buff, ' '));
        sprintf(retString, "Set log level as %u.", logLevel);
        QXUtil_LogSetLevel(logLevel);
    }
    else
    {
        ret = -QX_ENOSYS;
        goto CommonReturn;
    }

CommonReturn:
    if (retString && strlen(retString))
    {
        len = send(Fd, retString, strlen(retString) + 1, 0);
        if (len <= 0)
        {
            LogErr("Send CmdLine reply failed!");
        }
    }

    if (retString) {
        free(retString);
    }
    
    if (ret)
    {
        QX_UATOMIC_DEC(&sg_CmdLineWorker.Stat.ExecCnt);
        LogErr("%d:%s", ret, QX_StrErr(ret));
    }
    return ret;
}

static void*
_CmdServerWorkerProc(
    void* arg
    )
{
    int ret = QX_SUCCESS;
    int serverFd = -1;
    int epollFd = -1;
    int event_count = 0;
    struct epoll_event event, waitEvents[QX_BUFF_128];
    int loop = 0;
    int clientFd[QX_MAX_CLIENT_NUM_PER_SERVER] = {0};
    int clientFdCnt = 0;
    char recvBuff[QX_BUFF_128] = {0};
    int recvLen = 0;
    char* roleName = (char*)arg;

    ret = _CmdServerInit(&serverFd, &epollFd, roleName);
    if (ret)
    {
        LogErr("Cmd server init failed %d", ret);
        fflush(stdout);
        return NULL;
    }

    UNUSED(arg);
    /* recv */
    while (!sg_CmdLineWorker.Exit)
    {
        event_count = epoll_wait(epollFd, waitEvents, QX_BUFF_128, 100); //timeout 0.1s
        if (event_count == -1)
        {
            LogErr("Epoll wait failed! (%d:%s)", errno, QX_StrErr(errno));
            goto CommonReturn;
        }
        else if (event_count == 0)
        {
            continue;
        }
        for(loop = 0; loop < event_count; loop ++)
        {
            if (waitEvents[loop].data.fd == serverFd)
            {
                int tmpClientFd = -1;
                struct sockaddr tmpClientaddr;
                socklen_t tmpClientLen;
                QX_UATOMIC_INC(&sg_CmdLineWorker.Stat.ConnectCnt);
                tmpClientFd = accept(serverFd, &tmpClientaddr, &tmpClientLen);
                if (tmpClientFd != -1)
                {
                    int flags = fcntl(tmpClientFd, F_GETFL, 0);
                    fcntl(tmpClientFd, F_SETFL, flags | O_NONBLOCK);    // set non block

                    event.data.fd = tmpClientFd;
                    event.events = EPOLLIN | EPOLLET;  
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, tmpClientFd, &event);
                    clientFd[clientFdCnt ++] = tmpClientFd;
                }
            }
            else if (waitEvents[loop].events & EPOLLIN)
            {
                memset(recvBuff, 0, sizeof(recvBuff));
                recvLen = recv(waitEvents[loop].data.fd, recvBuff, sizeof(recvBuff), 0);
                if (recvLen > 0)
                {
                    LogInfo("Recv cmdline msg [%s]", recvBuff);
                    ret = _CmdServerHandleMsg(waitEvents[loop].data.fd, recvBuff);
                    if (ret)
                    {
                        LogErr("Handle msg filed %d", ret);
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, waitEvents[loop].data.fd, NULL);
                        close(waitEvents[loop].data.fd);
                    }
                }
                else if (recvLen == 0)
                {
                    LogInfo("CmdLineClient closed connection.");
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, waitEvents[loop].data.fd, NULL);
                    close(waitEvents[loop].data.fd);
                }
                else
                {
                    ret = -QX_EIO;
                    LogErr("Recv in %d failed %d, errno %s:%d", waitEvents[loop].data.fd, recvLen, errno, strerror(errno));
                    goto CommonReturn;
                }
            }
            else if (waitEvents[loop].events & EPOLLERR || waitEvents[loop].events & EPOLLHUP)
            {
                LogInfo("%d error happen!", waitEvents[loop].data.fd);
                epoll_ctl(epollFd, EPOLL_CTL_DEL, waitEvents[loop].data.fd, NULL);
                close(waitEvents[loop].data.fd);
                continue;
            }
        }
    }
    
CommonReturn:
    for(loop = 0; loop < clientFdCnt; loop ++)
    {
        if (clientFd[loop] > 0)
            close(clientFd[loop]);
    }
    if (serverFd > 0)
        close(serverFd);
    if (epollFd > 0)
        close(epollFd);
    return NULL;
}

static
BOOL 
_CmdLineInputIsOk(
    char** Argv,
    int Argc
    )
{
    int loop = 0;
    QX_CMD_EXTERNAL_NODE *externLoop = NULL, *externTmp = NULL;
    
    if (!Argv || Argc < 1 || !Argv[1])
    {
        return FALSE;
    }
    
    for(loop = 0; loop < QX_CMD_TYPE_MAX_UNUSED; loop ++)
    {
        if (strcasecmp(Argv[1], sg_CmdLineCont[loop].Opt) == 0 && Argc == sg_CmdLineCont[loop].Argc)
            return TRUE;
    }

    if (sg_CmdExternalWorker.Inited && !QX_LIST_IS_EMPTY(sg_CmdExternalWorker.ExternalCmdHead))
    {
        pthread_spin_lock(&sg_CmdExternalWorker.Lock);
        QX_LIST_FOR_EACH(sg_CmdExternalWorker.ExternalCmdHead, externLoop, externTmp, QX_CMD_EXTERNAL_NODE, List)
        {
            if (strcasecmp(Argv[1], externLoop->Content.Opt) == 0 && Argc == externLoop->Content.Argc)
                return TRUE;
        }
        pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
    }

    return FALSE;
}

static int
_CmdClientWorkerProc(
    char** Argv,
    int Argc,
    char* RoleName
    )
{
    int ret = QX_SUCCESS;
    int clientFd = -1;
    int32_t reuseable = 1; // set port reuseable when fd closed
    char cmd[QX_BUFF_64] = {0};
    char unixDomainPath[QX_BUFF_128] = {0};
    size_t len = 0;
    struct sockaddr_un serverAddr;
    struct timeval tv;

    if (!Argv || (Argc != 2 && Argc != 3) || !_CmdLineInputIsOk(Argv, Argc) || !RoleName)
    {
        ret = -QX_EINVAL;
        _CmdLineUsage(NULL);
        goto CommonReturn;
    }
    if (strcasecmp(sg_CmdLineCont[QX_CMD_TYPE_START].Opt, Argv[1]) == 0)
    {
        printf("Already running!\n");
        goto CommonReturn;
    }

    if (Argc == 2)
    {
        snprintf(cmd, sizeof(cmd), "%s", Argv[1]);
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "%s %s", Argv[1], Argv[2]);
    }

    clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(0 > clientFd)
    {
        printf("Create socket failed\n");
        goto CommonReturn;
    }
    (void)setsockopt(clientFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable));
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommonReturn;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommonReturn;
    }

    len = snprintf(unixDomainPath + 1, sizeof(unixDomainPath) - 1, "%s.domain", RoleName) + 1;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    memcpy(serverAddr.sun_path, unixDomainPath, sizeof(serverAddr.sun_path) > len ? len : sizeof(serverAddr.sun_path));
    if(connect(clientFd, (void *)&serverAddr, sizeof(serverAddr)))
    {
        printf("Connect failed\n");
        goto CommonReturn;
    }

    ret = send(clientFd, cmd, strlen(cmd) + 1, 0);
    if (ret > 0)
    {
        ret = QX_SUCCESS;
    }
    else
    {
        ret = -errno;
        printf("Send cmdline failed, %d %s\n", ret, QX_StrErr(ret));
        goto CommonReturn;
    }

    char *recvBuff = NULL;
    recvBuff = (char*)malloc(CMDLINE_BUFFER_LEN);
    if (!recvBuff) {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    memset(recvBuff, 0, CMDLINE_BUFFER_LEN);
    ret = recv(clientFd, recvBuff, CMDLINE_BUFFER_LEN, 0);
    if (ret > 0)
    {
        ret = QX_SUCCESS;
        printf("%s\n", recvBuff);
    }
    else
    {
        ret = -errno;
        printf("Recv cmdline reply failed, %d %s\n", ret, QX_StrErr(ret));
        goto CommonReturn;
    }

CommonReturn:
    if (recvBuff) {
        free(recvBuff);
    }
    return ret;
}

int
QXUtil_CmdLineModuleInit(
    QX_UTIL_CMDLINE_MODULE_INIT_ARG *InitArg
    )
{
    int ret = QX_SUCCESS;
    int pidFd = -1;
    char path[QX_BUFF_128] = {0};
    int isRunning = 0;
    
    if (!InitArg || (InitArg->Argc != 2 && InitArg->Argc != 3) || 
        !InitArg->Argv || !strlen(InitArg->RoleName) || !InitArg->ExitFunc)
    {
        goto CommonErr;
    }

    sg_CmdLineWorker.ExitCb = InitArg->ExitFunc;
    snprintf(path, sizeof(path), "/run/%s.pid", InitArg->RoleName);
    pidFd = QXUtil_OpenPidFile(path);
    if (pidFd < 0)
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    isRunning = QXUtil_IsProcessRunning(pidFd);
    if (isRunning == -1)
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    sg_CmdLineWorker.Role = isRunning ? QX_CMDLINE_ROLE_CLT : QX_CMDLINE_ROLE_SVR;
    
    if (strcasecmp(InitArg->Argv[1], sg_CmdLineCont[QX_CMD_TYPE_HELP].Opt) == 0)
    {
        goto CommonErr;
    }
    else if (strcasecmp(InitArg->Argv[1], sg_CmdLineCont[QX_CMD_TYPE_START].Opt) != 0 && 
            QX_CMDLINE_ROLE_SVR == sg_CmdLineWorker.Role)
    {
        if (strcasecmp(InitArg->Argv[1], sg_CmdLineCont[QX_CMD_TYPE_STOP].Opt) == 0)
        {
            printf("%s is not running!\n", InitArg->RoleName);
        }
        goto CommonErr;
    }

    switch (sg_CmdLineWorker.Role)
    {
        case QX_CMDLINE_ROLE_SVR:
            QXUtil_MakeDaemon();
            ret = QXUtil_SetPidIntoFile(pidFd);
            if (ret)
            {
                goto CommonReturn;
            }
            QXUtil_CloseStdFds();
            /* start worker pthread */
            sg_CmdLineWorker.Thread = (pthread_t*)calloc(sizeof(pthread_t), 1);
            if (!sg_CmdLineWorker.Thread)
            {
                ret = -QX_ENOMEM;
                LogErr("Apply memory failed!");
                goto CommonReturn;
            }
            sg_CmdLineWorker.Exit = FALSE;
            ret = pthread_create(sg_CmdLineWorker.Thread, NULL, _CmdServerWorkerProc, (void*)InitArg->RoleName);
            if (ret) 
            {
                LogErr("Failed to create thread");
                goto CommonReturn;
            }
            break;
        case QX_CMDLINE_ROLE_CLT:
            (void)_CmdClientWorkerProc(InitArg->Argv, InitArg->Argc, InitArg->RoleName);
            ret = -QX_ERR_EXIT_WITH_SUCCESS;
            goto CommonReturn;
        default:
            ret = EINVAL;
            LogErr("Role %d invalid!", sg_CmdLineWorker.Role);
            break;
    }
    goto CommonReturn;

CommonErr:
    _CmdLineUsage(InitArg->RoleName);
    ret = -QX_ERR_EXIT_WITH_SUCCESS;
CommonReturn:
    return ret;
}

void
QXUtil_CmdLineModuleExit(
    void
    )
{
    QX_CMD_EXTERNAL_NODE *loop = NULL, *tmp = NULL;
    
    if (!sg_CmdLineWorker.Exit)
    {
        sg_CmdLineWorker.Exit = TRUE;
        if (sg_CmdLineWorker.Thread)
        {
            pthread_join(*sg_CmdLineWorker.Thread, NULL);
            free(sg_CmdLineWorker.Thread);
            sg_CmdLineWorker.Thread = NULL;
        }
    }
    if (sg_CmdExternalWorker.Inited && !QX_LIST_IS_EMPTY(sg_CmdExternalWorker.ExternalCmdHead))
    {
        pthread_spin_lock(&sg_CmdExternalWorker.Lock);
        QX_LIST_FOR_EACH(sg_CmdExternalWorker.ExternalCmdHead, loop, tmp, QX_CMD_EXTERNAL_NODE, List)
        {
            QX_LIST_DEL_NODE(&loop->List);
            free(loop);
        }
        pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
        pthread_spin_destroy(&sg_CmdExternalWorker.Lock);
    }
}

// optional, for special users
int
QXUtil_CmdExternalRegister(
    __in QX_UTIL_CMD_EXTERNAL_CONT Cont
    )
{
    int ret = QX_SUCCESS;
    QX_CMD_EXTERNAL_NODE *node = NULL;

    if (sg_CmdLineWorker.Exit != TRUE)
    {
        // has to register before cmd module init
        ret = -QX_ENOSYS;
        goto CommonReturn;
    }

    if (!sg_CmdExternalWorker.Inited)
    {
        pthread_spin_init(&sg_CmdExternalWorker.Lock, PTHREAD_PROCESS_PRIVATE);
        pthread_spin_lock(&sg_CmdExternalWorker.Lock);
        if (!sg_CmdExternalWorker.ExternalCmdHead)  // prevent multi entering
        {
            sg_CmdExternalWorker.ExternalCmdHead = (QX_LIST_NODE*)calloc(sizeof(QX_LIST_NODE), 1);
            if (!sg_CmdExternalWorker.ExternalCmdHead)
            {
                ret = -QX_ENOMEM;
                pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
                goto CommonReturn;
            }
            QX_LIST_NODE_INIT(sg_CmdExternalWorker.ExternalCmdHead);
        }
        sg_CmdExternalWorker.Inited = TRUE;
        pthread_spin_unlock(&sg_CmdExternalWorker.Lock);
    }

    if (Cont.Argc <= 1 || 
        strnlen(Cont.Opt, sizeof(Cont.Opt)) == 0 ||
        strnlen(Cont.Help, sizeof(Cont.Help)) == 0 ||
        !Cont.Cb)
    {
        ret = -QX_EINVAL;
        goto CommonReturn;
    }

    node = (QX_CMD_EXTERNAL_NODE*)calloc(sizeof(QX_CMD_EXTERNAL_NODE), 1);
    if (!node)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }
    QX_LIST_NODE_INIT(&node->List);
    memcpy(&node->Content, &Cont, sizeof(QX_UTIL_CMD_EXTERNAL_CONT));
    pthread_spin_lock(&sg_CmdExternalWorker.Lock);
    QX_LIST_ADD_TAIL(&node->List, sg_CmdExternalWorker.ExternalCmdHead);
    pthread_spin_unlock(&sg_CmdExternalWorker.Lock);

CommonReturn:
    return ret;
}

int
QXUtil_CmdLineModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = QX_SUCCESS;

    *Offset += snprintf(Buff + *Offset , BuffMaxLen - *Offset, 
            "<%s:[IsRunning:%s CmdConnectCnt:%d CmdExecCnt:%d]>", QXUtil_ModuleNameByEnum(QX_UTIL_MODULES_ENUM_CMDLINE), 
                sg_CmdLineWorker.Exit ? "FALSE" : "TRUE", sg_CmdLineWorker.Stat.ConnectCnt, sg_CmdLineWorker.Stat.ExecCnt); 
    return ret;
}

