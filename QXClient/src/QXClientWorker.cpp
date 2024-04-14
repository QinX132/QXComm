#include <curl/curl.h>

#include "QXSCMsg.pb.h"
#include "QXClientWorker.h"
#include "QXUtilsCommonUtil.h"
#include "QXClientMsgBussiness.h"

using namespace std;

static string sg_QXCStatString[QXC_WORKER_STATS_MAX] = {
    [QXC_WORKER_STATS_UNSPEC]          =   "Unknown",
    [QXC_WORKER_STATS_INITED]          =   "Inited",
    [QXC_WORKER_STATS_CONNECTED]       =   "Connected",
    [QXC_WORKER_STATS_REGISTERED]      =   "Active",
    [QXC_WORKER_STATS_DISCONNECTED]    =   "Disconnected",
    [QXC_WORKER_STATS_EXIT]            =   "Exited",
};

QX_ERR_T QXClientWorker::ConnectServer(void) {
    QX_ERR_T ret = QX_SUCCESS;
    bool connectSuccess = false;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    for(size_t loop = CurrentServerPos >= 0 ? CurrentServerPos + 1 : rand() % InitParam.Servers.size(); 
        loop < InitParam.Servers.size(); 
        loop ++) {
        ret = QXUtil_ParseStringToIpv4AndPort(InitParam.Servers[loop].Addr.c_str(), InitParam.Servers[loop].Addr.length(), &ip, &port);
        if (ret < QX_SUCCESS || ip == 0 || port == 0) {
            LogErr("Parse %s failed!", InitParam.Servers[loop].Addr.c_str());
            continue;
        }
        serverAddr.sin_addr.s_addr = htonl(ip);
        serverAddr.sin_port = htons(port);
        if (connect(WorkerFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            LogErr("Connect to %s(%u:%u) failed, errno %d:%s, try next one", InitParam.Servers[loop].Addr.c_str(), ip, port, errno, QX_StrErr(errno));
            continue;
        } else {
            connectSuccess = true;
            CurrentServerPos = loop;
            break;
        }
    }

    if (!connectSuccess) {
        LogErr("Connect failed!");
        ret = -QX_ENOTCONN;
        goto CommRet;
    } else {
        LogDbg("Connect success! using %s", InitParam.Servers[CurrentServerPos].Addr.c_str());
        State = QXC_WORKER_STATS_CONNECTED;
        RegisterRetried = 0;
        AddRecvEvent();
    }

CommRet:
    return ret;
}

QX_ERR_T QXClientWorker::InitWorkerFd(void) {
    QX_ERR_T ret = QX_SUCCESS;
    struct timeval tv;
    int32_t reuseable = 1; // set port reuseable when fd closed
    // socket
    WorkerFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WorkerFd < 0) {
        LogErr("Socket failed!");
        ret = -QX_EBADFD; 
        goto CommRet;
    }
    // reuse
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommErr;
    }
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommErr;
    }
    goto CommRet;
    
CommErr:
    close(WorkerFd);
    WorkerFd = -1;
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
    // check register send
    if (RegisterRetried >= 3) {
        RemoveRecvEvent();
        RecreateWorkerFd();
        State = QXC_WORKER_STATS_DISCONNECTED;
        goto CommRet;
    }
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
    ret = QXUtil_SendQMsg(WorkerFd, qMsg);
    if (ret < QX_SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", msgPayload.ShortDebugString().c_str());
    RegisterRetried ++;

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
    LogDbg("event base loop break here!");
    event_base_free(worker->EventBase);
    worker->EventBase = NULL;

    return NULL;
}

void QXClientWorker::AddRecvEvent(void) {
    if (!RecvEvent && EventBase) {
        RecvEvent = event_new(EventBase, WorkerFd, EV_READ|EV_PERSIST, _RecvMsg, (void*)this);
        event_add(RecvEvent, NULL);
        LogDbg("Adding recv event into base......");
    }
}

void QXClientWorker::RemoveRecvEvent(void) {
    if (RecvEvent) {
        event_del(RecvEvent);
        event_free(RecvEvent);
        RecvEvent = NULL;
        LogDbg("Recv event deleted from base.");
    }
}

void QXClientWorker::RecreateWorkerFd(void) {
    close(WorkerFd);
    WorkerFd = -1;
    (void)InitWorkerFd();
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
        worker->RecreateWorkerFd();
        worker->State = QXC_WORKER_STATS_DISCONNECTED;
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
            case QXC_WORKER_STATS_INITED:
            case QXC_WORKER_STATS_DISCONNECTED:
                ret = worker->ConnectServer();
                if (ret < QX_SUCCESS) {
                    LogErr("Connect failed! ret %d", ret);
                }
                ret = worker->RegisterToServer();
                if (ret < QX_SUCCESS) {
                    LogErr("Send register msg failed! ret %d", ret);
                }
                break;
            case QXC_WORKER_STATS_CONNECTED:
                ret = worker->RegisterToServer();
                if (ret < QX_SUCCESS) {
                    LogErr("Send register msg failed! ret %d", ret);
                }
                LogInfo("Worker in stat <%s>.", sg_QXCStatString[worker->State].c_str());
                break;
            case QXC_WORKER_STATS_REGISTERED:
                break;
            case QXC_WORKER_STATS_EXIT:
            default:
                LogErr("In stat %d, exiting stat machine!", worker->State);
                return NULL;
        }
        if (worker->State != QXC_WORKER_STATS_REGISTERED)
            LogInfo("Worker in stat <%s>.", sg_QXCStatString[worker->State].c_str());
        sleep(3);
    }

    return NULL;
}

static void _Keepalive(evutil_socket_t Fd, short Event, void *Arg) {
    UNUSED(Fd);
    UNUSED(Event);
    UNUSED(Arg);
}

QX_ERR_T QXClientWorker::InitEventBaseAndRun(void) {
    QX_ERR_T ret = QX_SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;
    struct timeval tv;

    if (EventBase != NULL) {
        goto CommRet;
    }

    EventBase = event_base_new();
    if (!EventBase) {
        ret = -QX_ENOMEM;
        LogErr("Create worker event base failed!");
        goto CommRet;
    }
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    KeepaliveEvent = event_new(EventBase, -1, EV_READ|EV_PERSIST, _Keepalive, NULL);
    if (!KeepaliveEvent) {
        ret = -QX_ENOMEM;
        LogErr("Create keepalive event base failed!");
        goto CommRet;
    }
    event_add(KeepaliveEvent, &tv);
    event_active(KeepaliveEvent, 0, EV_READ);   // Must be actively activated once, otherwise it will not run

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&MsgThreadId, &attr, _MsgThreadFn, (void*)this);
    if (0 != result) {
        ret = -QX_EIO;
        LogErr("Create worker thread failed! errno %d", errno);
        event_free(KeepaliveEvent);
        KeepaliveEvent = NULL;
        event_base_free(EventBase);
        EventBase = NULL;
        goto CommRet;
    }

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

QX_ERR_T QXClientWorker::Init(QXC_WORKER_INIT_PARAM InitParam){
    QX_ERR_T ret = QX_SUCCESS;

    if (QXC_IS_INITED(State)) {
        goto CommRet;
    }
    
    QXClientWorker::InitParam = InitParam;
    MsgHandler = new QXClientMsgHandler;
    if (MsgHandler == NULL) {
        LogErr("Init MsgHandler failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitWorkerFd();
    if (ret < QX_SUCCESS) {
        LogErr("Init fd failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitEventBaseAndRun();
    if (ret < QX_SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    State = QXC_WORKER_STATS_INITED;
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
        if (WorkerFd >= 0) {
            close(WorkerFd);
            WorkerFd = -1;
        }
        if (MsgHandler) {
            delete MsgHandler;
            MsgHandler = NULL;
        }
        if (EventBase) {
            RemoveRecvEvent();
            event_base_loopbreak(EventBase);
            if (KeepaliveEvent) {
                event_del(KeepaliveEvent);
                event_free(KeepaliveEvent);
                KeepaliveEvent = NULL;
            }
        }
        State = QXC_WORKER_STATS_EXIT;
    }
}

QXClientWorker::QXClientWorker() :
    WorkerFd(-1), State(QXC_WORKER_STATS_UNSPEC), EventBase(NULL), RecvEvent(NULL), MsgHandler(NULL), RegisterRetried(0), CurrentServerPos(-1){}

QXClientWorker::~QXClientWorker() {}
