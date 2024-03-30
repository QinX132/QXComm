#include "QXSCMsg.pb.h"
#include "QXClientWorker.h"
#include "QXUtilsCommonUtil.h"
#include "QXClientMsgBussiness.h"

using namespace std;

static string sg_QXCStatString[QXC_STATS_MAX] = {
    [QXC_STATS_UNSPEC]          =   "Unknown",
    [QXC_STATS_INITED]          =   "Inited",
    [QXC_STATS_CONNECTED]       =   "Connected",
    [QXC_STATS_REGISTERED]      =   "Active",
    [QXC_STATS_DISCONNECTED]    =   "Disconnected",
    [QXC_STATS_EXIT]            =   "Exited",
};

QX_ERR_T QXClientWorker::ConnectServer(void) {
    QX_ERR_T ret = QX_SUCCESS;
    bool connectSuccess = false;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    for(auto serverConf:Param.Servers) {
        ret = QXUtil_ParseStringToIpv4AndPort(serverConf.Addr.c_str(), serverConf.Addr.length(), &ip, &port);
        if (ret < QX_SUCCESS || ip == 0 || port == 0) {
            LogErr("Parse %s failed!", serverConf.Addr.c_str());
            continue;
        }
        serverAddr.sin_addr.s_addr = htonl(ip);
        serverAddr.sin_port = htons(port);
        if (connect(ClientFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            LogErr("Connect to %s(%u:%u) failed, errno %d, try next one", serverConf.Addr.c_str(), ip, port, errno);
            continue;
        } else {
            connectSuccess = true;
            CurrentServer = serverConf;
            break;
        }
    }

    if (!connectSuccess) {
        LogErr("Connect failed!");
        ret = -QX_ENOTCONN;
        goto CommRet;
    } else {
        LogDbg("Connect success! using %s", CurrentServer.Addr.c_str());
        State = QXC_STATS_CONNECTED;
        AddRecvEvent();
    }

CommRet:
    return ret;
}

QX_ERR_T QXClientWorker::InitClientFd(void) {
    QX_ERR_T ret = QX_SUCCESS;
    struct timeval tv;
    int32_t reuseable = 1; // set port reuseable when fd closed
    // socket
    ClientFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientFd < 0) {
        LogErr("Socket failed!");
        ret = -QX_EBADFD; 
        goto CommRet;
    }
    // reuse
    if (setsockopt(ClientFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommErr;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommErr;
    }
    goto CommRet;
    
CommErr:
    close(ClientFd);
    ClientFd = -1;
CommRet:
    return ret;
} 

QX_ERR_T QXClientWorker::RegisterToServer(void) {
    QX_ERR_T ret = QX_SUCCESS;
    QXSCMsg::MsgPayload msgPayload;
    QX_UTIL_Q_MSG *qMsg = NULL;
    std::string serializedData;
    // init payload
    MsgHandler->ProtoInitMsg(this, msgPayload);
    // create registerproto
    MsgHandler->CreateRegisterProtoMsg(this, *msgPayload.mutable_msgbase());
    // msg pre send
    MsgHandler->ProtoPreSend(msgPayload);
    // send msg
    serializedData = msgPayload.SerializeAsString();
    qMsg = QXUtil_NewSendQMsg(serializedData.size());
    if (!qMsg) {
        ret = -QX_ENOMEM;
        LogErr("Get msg mem failed!");
        goto CommRet;
    }
    memcpy(qMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    ret = QXUtil_SendQMsg(ClientFd, qMsg);
    if (ret < QX_SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", msgPayload.ShortDebugString().c_str());

CommRet:
    if (qMsg) 
        QXUtil_FreeSendQMsg(qMsg);
    MsgHandler->ProtoRelease(msgPayload);
    return ret;
}

void*
QXClientWorker::_MsgThreadFn(void *Arg) {
    QXClientWorker *worker = (QXClientWorker*)Arg;
    
    event_base_dispatch(worker->EventBase);
    // break at here
    event_base_free(worker->EventBase);
    worker->EventBase = NULL;

    return NULL;
}

void QXClientWorker::AddRecvEvent(void) {
    if (!RecvEvent) {
        RecvEvent = event_new(EventBase, ClientFd, EV_READ|EV_PERSIST, _RecvMsg, (void*)this);
        event_add(RecvEvent, NULL);
    }
}

void QXClientWorker::RemoveRecvEvent(void) {
    if (RecvEvent) {
        event_del(RecvEvent);
        event_free(RecvEvent);
        RecvEvent = NULL;
    }
}

void QXClientWorker::_RecvMsg(evutil_socket_t Fd, short Event, void *Arg) {
    QXClientWorker *worker = (QXClientWorker*)Arg;
    QX_ERR_T ret = QX_SUCCESS;
    QX_UTIL_Q_MSG *recvMsg = NULL;
    QXSCMsg::MsgPayload msgPayload;
    UNUSED(Event);

    recvMsg = QXUtil_NewRecvQMsg();
    if (!recvMsg) {
        LogErr("Apply failed!");
        return ;
    }
    ret = QXUtil_RecvQMsg(Fd, recvMsg);
    if (ret < 0) {
        LogErr("recv failed!");
        worker->RemoveRecvEvent();
        goto CommRet;
    }
    if (!msgPayload.ParseFromArray(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
        LogErr("parse form array failed!");
        goto CommRet;
    }
    LogInfo("Recv msg: %s", msgPayload.ShortDebugString().c_str());
    ret = worker->MsgHandler->DispatchMsg(worker, msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (recvMsg)
        QXUtil_FreeRecvQMsg(recvMsg);
    return ;
}

void*
QXClientWorker::_StateMachineThreadFn(void *Arg) {
    QXClientWorker *worker = (QXClientWorker*)Arg;
    QX_ERR_T ret = QX_SUCCESS;
    
    while(QXC_IS_INITED(worker->State)) {
        switch (worker->State) {
            case QXC_STATS_INITED:
            case QXC_STATS_DISCONNECTED:
                ret = worker->ConnectServer();
                if (ret < QX_SUCCESS) {
                    LogErr("Connect failed! ret %d", ret);
                }
                break;
            case QXC_STATS_CONNECTED:
                ret = worker->RegisterToServer();
                if (ret < QX_SUCCESS) {
                    LogErr("Send register msg failed! ret %d", ret);
                }
                break;
            case QXC_STATS_REGISTERED:
                break;
            case QXC_STATS_EXIT:
            default:
                LogErr("In stat %d, exiting stat machine!", worker->State);
                return NULL;
        }
        LogErr("Client in stat <%s>.", sg_QXCStatString[worker->State].c_str());
        sleep(3);
    }

    return NULL;
}

QX_ERR_T QXClientWorker::InitEventBaseAndRun(void) {
    QX_ERR_T ret = QX_SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;

    EventBase = event_base_new();
    if(!EventBase)
    {
        ret = -QX_ENOMEM;
        LogErr("New event base failed!\n");
        goto NewEventBaseErr;
    }
    AddRecvEvent();

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&MsgThreadId, &attr, _MsgThreadFn, (void*)this);
    if (result != 0) {
        ret = -QX_EIO;
        LogErr("create thread failed! ret %d errno %d\n", result, errno);
        goto CommRet;
    }

    goto CommRet;

NewEventBaseErr:
    EventBase = NULL;
CommRet:
    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

QX_ERR_T QXClientWorker::StartStateMachine(void) {
    QX_ERR_T ret = QX_SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&StateMachineThreadId, &attr, _StateMachineThreadFn, (void*)this);
    if (result != 0) {
        ret = -QX_EIO;
        LogErr("create thread failed! ret %d errno %d\n", result, errno);
    }

    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

QX_ERR_T QXClientWorker::Init(QX_CLIENT_WORKER_INIT_PARAM InitParam){
    QX_ERR_T ret = QX_SUCCESS;

    if (QXC_IS_INITED(State)) {
        goto CommRet;
    }
    
    State = QXC_STATS_INITED;
    Param = InitParam;
    MsgHandler = new QXClientMsgHandler;
    if (MsgHandler == NULL) {
        LogErr("Init MsgHandler failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitClientFd();
    if (ret < QX_SUCCESS) {
        LogErr("Init fd failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitEventBaseAndRun();
    if (ret < QX_SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    ret = StartStateMachine();
    if (ret < QX_SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    goto CommRet;

InitErr:
    Exit();
CommRet:
    return ret;
}

void QXClientWorker::Exit(void) {
    if (QXC_IS_INITED(State)) {
        if (ClientFd) {
            close(ClientFd);
            ClientFd = -1;
        }
        if (MsgHandler) {
            delete MsgHandler;
            MsgHandler = NULL;
        }
        if (EventBase) {
            RemoveRecvEvent();
            event_base_loopbreak(EventBase);
        }
        State = QXC_STATS_EXIT;
    }
}

QXClientWorker::QXClientWorker() :
    ClientFd(-1), State(QXC_STATS_UNSPEC), EventBase(NULL), RecvEvent(NULL), MsgHandler(NULL){}

QXClientWorker::~QXClientWorker() {
    if (ClientFd != -1) {
        close(ClientFd);
    }
}