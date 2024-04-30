#include "QXUtilsCommonUtil.h"

#include <sys/resource.h>
#include <sys/time.h>

void
QXUtil_MakeDaemon(
    void
    )
{
    pid_t pid = fork();
    if (pid != 0)
    {
        exit(0);
    }
    setsid();
    return;
}

int
QXUtil_IsProcessRunning(
    int Fd
    )
{
    int ret = QX_SUCCESS;

    /*lock pid file*/
    ret = flock(Fd, LOCK_EX | LOCK_NB);
    if (ret < 0)
    {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
        {
            ret = 1;
        }
    }
    return ret;
}

int
QXUtil_OpenPidFile(
    char* Path
    )
{
    return Path ? open(Path, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) : -1;
}

int
QXUtil_SetPidIntoFile(
    int Fd
    )
{
    int32_t ret = QX_SUCCESS;
    char buf[QX_BUFF_32] = {0};
    ssize_t len = 0;

    ftruncate(Fd, 0);
    sprintf(buf, "%u", getpid());
    len = write(Fd, buf, strlen(buf) + 1);
    if (len != (ssize_t)(strlen(buf) + 1))
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}

void 
QXUtil_CloseStdFds(
    void
    )
{
    int32_t fd = -1;
    (void)close(STDERR_FILENO);
    (void)close(STDOUT_FILENO);
    (void)close(STDIN_FILENO);
    fd = open("/dev/null", O_RDONLY);   /* fd == 0: stdin.  */
    fd = open("/dev/null", O_WRONLY);   /* fd == 1: stdout. */
    fd = dup(fd);
}

int
QXUtil_GetCpuTime(
    uint64_t *TotalTime,
    uint64_t *IdleTime
    )
{
    int ret = QX_SUCCESS;
    FILE* fp = NULL;
    unsigned long long user = 0, nice = 0, system = 0, idle = 0;
    fp = fopen("/proc/stat", "r");
    if (!fp || !TotalTime || !IdleTime) 
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }

    if (fscanf(fp, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle) != 4) 
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }

    *TotalTime = user + nice + system + idle;
    *IdleTime = idle;

CommonReturn:
    if (fp)
    {
        fclose(fp);
    }
    return ret;
}

QX_ERR_T 
QXUtil_GetMemUsage(
    float *Usage
    )
{
    QX_ERR_T ret = QX_SUCCESS;
    FILE* fp = NULL;
    char buff[256] = {0};
    long unsigned int memTotal = 0, memFree = 0, buffers = 0, cached = 0, usedMem = 0;

    fp = fopen("/proc/meminfo", "r");
    if (!fp || !Usage) 
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }

    while (fgets(buff, sizeof(buff), fp)) {
        if (strncmp(buff, "MemTotal:", strlen("MemTotal:")) == 0) {
            sscanf(buff, "%*s %lu", &memTotal);
        } else if  (strncmp(buff, "MemFree:", strlen("MemFree:")) == 0) {
            sscanf(buff, "%*s %lu", &memFree);
        } else if  (strncmp(buff, "Buffers:", strlen("Buffers:")) == 0) {
            sscanf(buff, "%*s %lu", &buffers);
        } else if  (strncmp(buff, "Cached:", strlen("Cached:")) == 0) {
            sscanf(buff, "%*s %lu", &cached);
        } else {
            // do nothing
        }
        memset(buff, 0, sizeof(buff));
    }
    usedMem = memTotal - memFree - buffers - cached;
    *Usage = ((float)usedMem / (float)memTotal) * 100.0;

CommonReturn:
    if (fp)
    {
        fclose(fp);
    }
    return ret;
}

uint64_t 
QXUtil_htonll(
    uint64_t value
    )
{
    uint64_t high = (value >> 32) & 0xFFFFFFFF;
    uint64_t low = value & 0xFFFFFFFF;
    return ((uint64_t)htonl(high) << 32) | htonl(low);
}

uint64_t 
QXUtil_ntohll(
    uint64_t value
    )
{
    uint64_t high = (value >> 32) & 0xFFFFFFFF;
    uint64_t low = value & 0xFFFFFFFF;
    return ((uint64_t)ntohl(high) << 32) | ntohl(low);
}

void
QXUtil_ChangeCharA2B(
    char* String,
    size_t StringLen,
    char A,
    char B
    )
{
    while (StringLen --)
        if (*(String + StringLen) == A)
            *(String + StringLen) = B;
}

int
QXUtil_ParseStringToIpv4(
    const char* String,
    size_t StringLen,
    uint32_t *Ip
    )
{
    uint32_t ip1, ip2, ip3, ip4;
    if (!String || !StringLen || !Ip)
        return -1;
    if (sscanf(String, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4)
        return -1;

    *Ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

    return 0;
}

int
QXUtil_ParseStringToIpv4AndPort(
    const char* String,
    size_t StringLen,
    uint32_t *Ip,
    uint16_t *Port
    )
{
    uint32_t ip1, ip2, ip3, ip4, port;

    if (!String || !StringLen || !Ip || !Port)
        return -1;
    if (sscanf(String, "%u.%u.%u.%u:%u", &ip1, &ip2, &ip3, &ip4, &port) != 5)
        return -1;
    if (ip1 > 0xff || ip2 > 0xff || ip3 > 0xff || ip4 > 0xff || port > 0xffff)
        return -1;
    
    *Ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;
    *Port = port;

    return 0;
}
#include "QXUtilsLogIO.h"
void 
QXUtil_Hexdump(
    const char *Title, 
    unsigned char *Buff, 
    int Length
    )
{
    unsigned char *ptrTmp = Buff;
    int i = 0;
    int count = 0;
    char* logBuff = NULL;
    size_t logBuffLen = QX_BUFF_4096;
    size_t logBuffOffset = 0;

    logBuff = (char*)calloc(QX_BUFF_4096, sizeof(char));
    if (!logBuff) {
        goto CommRet;
    }

    logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, 
        "--------------	%s: (start @%p  length %4d)------------------\n", Title, Buff, Length);
    
    for(i = 0; i< Length; i++, count++)
    {
        if(i%16 == 0)
            logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, "%08x:	", count);
        logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, "%02x ", ptrTmp[i]);
        if(i%16 == 15 && i != Length - 1)
            logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, "\n");
    }
    logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, "\n");
    logBuffOffset += snprintf(logBuff + logBuffOffset, logBuffLen - logBuffOffset, "--------------	%s: (end)------------------\n", Title);
    
CommRet:
    if (logBuff) {
        LogInfo("\n%s", logBuff);
        free(logBuff);
    }
    return ;
}