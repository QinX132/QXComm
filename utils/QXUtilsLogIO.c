#include "QXUtilsLogIO.h"
#include "QXUtilsModuleHealth.h"

#include <sys/stat.h>

#define QX_LOG_DEFAULT_MEX_LEN                                      (50 * 1024 * 1024)  // 50 Mb
#define QX_LOG_DEFAULT_MEX_NUM                                      3
#define QX_LOG_DEFAULT_CHECK_FILE_INTERVL                           5 // second

static char *sg_LogLevelStr [QX_UTIL_LOG_LEVEL_MAX] = 
{
    [QX_UTIL_LOG_LEVEL_INFO] = "INFO",
    [QX_UTIL_LOG_LEVEL_DEBUG] = "DEBUG",
    [QX_UTIL_LOG_LEVEL_WARNING] = "WARN",
    [QX_UTIL_LOG_LEVEL_ERROR] = "ERROR",
};

typedef struct _QX_LOG_WORKER_STATS
{
    int LogPrinted;
    long long LogSize;
}
QX_LOG_WORKER_STATS;

typedef struct _QX_LOG_WORKER
{
    char RoleName[QX_ROLE_NAME_MAX_LEN];
    pthread_spinlock_t Lock;
    char LogPath[QX_BUFF_128];
    QX_UTIL_LOG_LEVEL LogLevel;
    BOOL Inited;
    FILE* Fp;
    QX_LOG_WORKER_STATS Stats;
    int LogMaxSize;
    int LogMaxNum;
    pthread_t CheckFileExitT;
}
QX_LOG_WORKER;


static QX_LOG_WORKER sg_LogWorker = {
        .RoleName = {0},
        .LogPath = {0},
        .LogLevel = QX_UTIL_LOG_LEVEL_INFO,
        .Inited = FALSE,
        .Fp = NULL,
        .Stats = {.LogPrinted = 0}
    };

static void*
_LogModuleCheckFile(
    void* Arg
    )
{
    struct stat statBuff;
    UNUSED(Arg);
    
    while(sg_LogWorker.Inited) {
        pthread_spin_lock(&sg_LogWorker.Lock);
        if (fstat(fileno(sg_LogWorker.Fp), &statBuff) != -1) {
            if (statBuff.st_nlink == 0) {
                fclose(sg_LogWorker.Fp);
                sg_LogWorker.Fp = fopen(sg_LogWorker.LogPath, "a");
            }
        }
        pthread_spin_unlock(&sg_LogWorker.Lock);
        sleep(QX_LOG_DEFAULT_CHECK_FILE_INTERVL);
    }

    return NULL;
}

int
QXUtil_LogModuleInit(
    QX_UTIL_LOG_MODULE_INIT_ARG *InitArg
    )
{
    int ret = QX_SUCCESS;

    if (sg_LogWorker.Inited)
    {
        goto CommonReturn;
    }
    
    if (!InitArg|| !strlen(InitArg->LogFilePath) || !strlen(InitArg->RoleName))
    {
        ret = -QX_EINVAL;
        LogErr("Too long role name!");
        goto CommonReturn;
    }
    strncpy(sg_LogWorker.RoleName, InitArg->RoleName, sizeof(sg_LogWorker.RoleName));
    strncpy(sg_LogWorker.LogPath, InitArg->LogFilePath, sizeof(sg_LogWorker.LogPath));
    sg_LogWorker.LogLevel = InitArg->LogLevel;
    sg_LogWorker.LogMaxSize = InitArg->LogMaxSize > 0 ? InitArg->LogMaxSize * 1024 * 1024 : QX_LOG_DEFAULT_MEX_LEN;
    sg_LogWorker.LogMaxNum = InitArg->LogMaxNum > 0 ? InitArg->LogMaxNum : QX_LOG_DEFAULT_MEX_NUM;
    pthread_spin_init(&sg_LogWorker.Lock, PTHREAD_PROCESS_PRIVATE);

    sg_LogWorker.Fp = fopen(sg_LogWorker.LogPath, "a");
    if (!sg_LogWorker.Fp)
    {
        ret = -QX_EIO;
        LogErr("Open file failed!");
        goto CommonReturn;
    }
    sg_LogWorker.Inited = TRUE;
    (void)pthread_create(&sg_LogWorker.CheckFileExitT, NULL, _LogModuleCheckFile, NULL);
    
CommonReturn:
    return ret;
}

static void
_LogRotate(
    void
    )
{
    char cmd[QX_BUFF_1024] = {0};
    
    pthread_spin_lock(&sg_LogWorker.Lock);
    fprintf(sg_LogWorker.Fp, "Lograting!\n");
    fsync(fileno(sg_LogWorker.Fp));
    fclose(sg_LogWorker.Fp);
    if (strlen(sg_LogWorker.LogPath))
    {
        snprintf(cmd, sizeof(cmd), 
                "tar -czvf \"$(dirname %s)/%s_$(date +%%Y%%m%%d%%H%%M%%S).tar.gz\" -C \"$(dirname %s)\" \"$(basename %s)\"", 
                    sg_LogWorker.LogPath, sg_LogWorker.RoleName, sg_LogWorker.LogPath, sg_LogWorker.LogPath);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "rm -f %s", sg_LogWorker.LogPath);
        system(cmd);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), "cd \"$(dirname %s)\" && ls -t %s*.tar.gz | tail -n +%d | xargs rm", 
                    sg_LogWorker.LogPath, sg_LogWorker.RoleName, sg_LogWorker.LogMaxNum + 1);
        system(cmd);
    }
    sg_LogWorker.Fp = fopen(sg_LogWorker.LogPath, "a");
    pthread_spin_unlock(&sg_LogWorker.Lock);
}

void
QXUtil_LogPrint(
    int Level,
    const char* File,
    const char* Function,
    int Line,
    const char* Fmt,
    ...
    )
{
    va_list args;
    char* fileNamePos = NULL;

    if (Level < (int)sg_LogWorker.LogLevel)
    {
        return;
    }

    if (!sg_LogWorker.Inited)
    {
        va_start(args, Fmt);
        printf("[%s-%d]:", Function, Line);
        vprintf(Fmt, args);
        va_end(args);
        printf("\n");
        return;
    }
    
    pthread_spin_lock(&sg_LogWorker.Lock);

    va_start(args, Fmt);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm* tm_info = localtime(&now);
    char timestamp[24] = {0};
    strftime(timestamp, sizeof(timestamp), "%Y/%m/%d_%H:%M:%S", tm_info);
    int milliseconds = tv.tv_usec / 1000;
#ifndef WINNT
    fileNamePos = strrchr(File, '/');
#else
    fileNamePos = strrchr(File, '\\')
#endif
    fileNamePos = fileNamePos != NULL ? fileNamePos + 1 : (char*)File;
    fprintf(sg_LogWorker.Fp, "[%s.%03d]<%s:%s>[%s:%s-%d]:", timestamp, milliseconds, sg_LogWorker.RoleName, sg_LogLevelStr[Level], fileNamePos, Function, Line);
    vfprintf(sg_LogWorker.Fp, Fmt, args);
    va_end(args);
    fprintf(sg_LogWorker.Fp, "\n");
    fflush(sg_LogWorker.Fp);

    sg_LogWorker.Stats.LogPrinted ++;
    sg_LogWorker.Stats.LogSize = ftell(sg_LogWorker.Fp);
    
    pthread_spin_unlock(&sg_LogWorker.Lock);
    
    if (sg_LogWorker.Stats.LogSize >= sg_LogWorker.LogMaxSize)
    {
        _LogRotate();
    }
}

void
QXUtil_LogModuleExit(
    void
    )
{
    if (sg_LogWorker.Inited)
    {
        pthread_spin_lock(&sg_LogWorker.Lock);
        sg_LogWorker.Inited = FALSE;
        pthread_spin_unlock(&sg_LogWorker.Lock);
        pthread_spin_destroy(&sg_LogWorker.Lock);
        fsync(fileno(sg_LogWorker.Fp));
        fclose(sg_LogWorker.Fp);  //we do not close fp
    }
}

int
QXUtil_LogModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = QX_SUCCESS;
    int len = 0;
    
    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, 
        "<%s:[LogPrinted:%d, LogSize:%lld Bytes]>", QXUtil_ModuleNameByEnum(QX_UTIL_MODULES_ENUM_LOG), 
        sg_LogWorker.Stats.LogPrinted, sg_LogWorker.Stats.LogSize);
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
QXUtil_LogSetLevel(
    uint32_t LogLevel
    )
{
    if (LogLevel <= QX_UTIL_LOG_LEVEL_ERROR)
    {
        sg_LogWorker.LogLevel = LogLevel;
    }
}
