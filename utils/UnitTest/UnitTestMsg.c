#include "UnitTest.h"
#include "QXUtilsMsg.h"
#include "QXUtilsMem.h"

#define UT_MSG_SOCKET_PORT                                      20000
#define UT_MSG_HEAD_TYPE                                        132
#define UT_MSG_HEAD_VER                                         0x10
#define UT_MSG_CONTENT                                          "UT_MSG_CONTENT"
#define UT_MSG_TAIL_SIGN                                        123
#define UT_MSG_CLIENTID                                         19001

static int
_UT_Msg_PreInit(
    void
    )
{
    return QXUtil_MemModuleInit();
}

static int
_UT_Msg_FinExit(
    void
    )
{
    return QXUtil_MemModuleExit();
}

static void*
_UT_Msg_Client(
    void* arg
    )
{
    int ret = QX_SUCCESS;
    int clientFd = -1;
    struct sockaddr_in serverAddr;
    int32_t reuseable = 1; // set port reuseable when fd closed
    QX_UTIL_MSG msg;
    UNUSED(arg);
    
    clientFd = socket(AF_INET, SOCK_STREAM, 0);
    if(0 > clientFd)
    {
        UTLog("Create socket failed!\n");
        goto CommonReturn;
    }
    (void)setsockopt(clientFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(UT_MSG_SOCKET_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(connect(clientFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        UTLog("Connect failed!\n");
        goto CommonReturn;
    }

    msg.Head.Type = UT_MSG_HEAD_TYPE;
    msg.Head.VerMagic = UT_MSG_HEAD_VER;
    msg.Head.ClientId = UT_MSG_CLIENTID;
    ret = QXUtil_FillMsgCont(&msg, (void*)UT_MSG_CONTENT, sizeof(UT_MSG_CONTENT));
    if (ret)
    {
        UTLog("Fill msg failed!\n");
        goto CommonReturn;
    }
    msg.Tail.Sign[0] = UT_MSG_TAIL_SIGN;
    ret = QXUtil_SendMsg(clientFd, &msg);
    if (ret)
    {
        UTLog("Recv msg failed!\n");
        goto CommonReturn;
    }
    UTLog("Send msg: Type = %u\n", msg.Head.Type);

CommonReturn:
    return NULL;
}

static int 
_UT_Msg_ForwardT(
    void
    )
{
    int ret = QX_SUCCESS;
    QX_UTIL_MSG *msg = NULL;
    pthread_t clientId;
    int serverFd = -1;
    int clientFd = -1;
    struct sockaddr_in localAddr;
    struct sockaddr tmpClientaddr;
    socklen_t tmpClientLen;
    int32_t reuseable = 1; // set port reuseable when fd closed

    memset(&clientId, 0, sizeof(clientId));
    
    ret = QXUtil_MsgModuleInit();
    if (ret)
    {
        UTLog("Init failed! ret %d %s\n", ret, QX_StrErr(ret));
        goto CommonReturn;
    }
    msg = QXUtil_NewMsg();
    if (!msg)
    {
        ret = -QX_ENOMEM;
        goto CommonReturn;
    }

    /* init server fd */
    // set reuseable
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    (void)setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable));
    // bind
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(UT_MSG_SOCKET_PORT);
    localAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(bind(serverFd, (struct sockaddr *)&localAddr, sizeof(localAddr)))
    {
        ret = -errno;
        UTLog("Bind failed!\n");
        goto CommonReturn;
    }
    ret = pthread_create(&clientId, NULL, _UT_Msg_Client, NULL);
    if (ret)
    {
        UTLog("Create thread failed! ret %d %s\n", ret, strerror(ret));
        goto CommonReturn;
    }
    if(listen(serverFd, 5))
    {
        ret = -errno;
        UTLog("Listen failed!\n");
        goto CommonReturn;
    }
    clientFd = accept(serverFd, &tmpClientaddr, &tmpClientLen);
    if(clientFd < 0)
    {
        ret = -QX_EIO;
        UTLog("accept failed!\n");
        goto CommonReturn;
    }

    ret = QXUtil_RecvMsg(clientFd, msg);
    if (ret)
    {
        UTLog("Recv msg failed!\n");
        goto CommonReturn;
    }
    
    UTLog("Recv msg: Type = %u\n", msg->Head.Type);

    if (msg->Head.Type != UT_MSG_HEAD_TYPE ||
        msg->Head.ContentLen != strlen(UT_MSG_CONTENT) + 1 ||
        msg->Head.VerMagic != UT_MSG_HEAD_VER ||
        strcmp((char*)(msg->Cont.VarLenCont), UT_MSG_CONTENT) ||
        msg->Tail.Sign[0] != UT_MSG_TAIL_SIGN ||
        msg->Head.ClientId != UT_MSG_CLIENTID)
    {
        ret = -QX_EIO;
        UTLog("Msg mismatch! Type%u ContentLen%u VerMagic%x Sign%u\n", 
            msg->Head.Type, msg->Head.ContentLen, msg->Head.VerMagic, msg->Tail.Sign[0]);
        goto CommonReturn;
    }

CommonReturn:
    if (msg)
    {
        QXUtil_FreeMsg(msg);
    }
    if (QXUtil_MsgModuleExit())
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
    assert(QX_SUCCESS == _UT_Msg_PreInit());
    assert(QX_SUCCESS == _UT_Msg_ForwardT());
    assert(QX_SUCCESS == _UT_Msg_FinExit());

    return 0;
}
