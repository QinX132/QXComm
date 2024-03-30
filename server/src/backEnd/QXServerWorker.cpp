#include "QXServerWorker.h"

using namespace std;

QX_ERR_T
QXServerWorker::InitServerFd(pair<uint16_t, uint16_t> PortRange, uint32_t Load) {
    struct sockaddr_in serverAddr;
    BOOL bindSuccess = FALSE;
    QX_ERR_T ret = QX_SUCCESS;
    uint16_t loop = 0;
    int option = 1, flags = 0;
    struct timeval tv;
    // socket
    ServerFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerFd < 0) {
        LogErr("Socket failed!");
        ret = -QX_EBADFD; 
        goto CreateFdFail;
    }
    // reuse && nonblock
    if (setsockopt(ServerFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags = fcntl(ServerFd, F_GETFL, 0);
    if (flags == -1) {
        LogErr("get flags failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags |= O_NONBLOCK;
    if (fcntl(ServerFd, F_SETFL, flags) == -1) {
        LogErr("set nonblock failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ServerFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommErr;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ServerFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommErr;
    }
    // bind port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    for(loop = PortRange.first; loop <= PortRange.second; loop ++) {
        serverAddr.sin_port = htons(loop);
        if (bind(ServerFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == 0) {
            bindSuccess = TRUE;
            break;
        }
    }
    if (!bindSuccess) {
        ret = -QX_EBADF; 
        goto CommErr;
    }
    // listen
    if (listen(ServerFd, Load) == -1) {
        LogErr("set nonblock failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }

    LogInfo("Create server worker, using port %u, fd %d.", loop, ServerFd);
    goto Success;
    
CommErr:
    close(ServerFd);
    ServerFd = -1;
CreateFdFail:
Success:
    return ret;
}

void
QXServerWorker::_ClientRecv(evutil_socket_t Fd, short Event, void *Arg) {
    QXServerWorker *worker = (QXServerWorker*)Arg;
    QX_UTIL_MSG msg;
    QX_ERR_T ret = QX_SUCCESS;
    UNUSED(Event);

    ret = QXUtil_RecvMsg(Fd, &msg);
    if (ret < 0) {
        pthread_spin_lock(&worker->Lock);
        event_del(worker->ClientMap[Fd]->RecvEvent);
        event_free(worker->ClientMap[Fd]->RecvEvent);
        worker->ClientMap[Fd]->RecvEvent = NULL;
        worker->ClientMap.erase(Fd);
        worker->ClientMap[Fd]->Registered = FALSE;
        close(worker->ClientMap[Fd]->Fd);
        worker->ClientMap[Fd]->Fd = -1;
        pthread_spin_unlock(&worker->Lock);
        return ;
    }

    pthread_spin_lock(&worker->Lock);
    if (!worker->ClientMap[Fd]->Registered) {
        if (msg.Head.Type != 1) {
            LogDbg("Not Registered, ignore msg type:%u", msg.Head.Type);
            return ;
        }
        else {
            pthread_spin_unlock(&worker->Lock);
            // registe check
            pthread_spin_lock(&worker->Lock);
            worker->ClientMap[Fd]->Registered = TRUE;
            worker->ClientMap[Fd]->ClientId = msg.Head.ClientId;
            worker->ClientMap[Fd]->RecvMsgCnt ++;
        }
    }
    pthread_spin_unlock(&worker->Lock);
    
}

void
QXServerWorker::_ServerAccept(evutil_socket_t Fd, short Event, void *Arg) {
    struct sockaddr_in clientAddr;
    socklen_t len = 0;
    int clientFd = -1;
    struct event *clientEvent = NULL;
    QXServerWorker *worker = (QXServerWorker*)Arg;
    QXS_CLIENT_NODE *clientNode = NULL;
    UNUSED(Event);

    clientFd = accept(Fd, (struct sockaddr*)&clientAddr, &len);
    if (clientFd < 0) {
        LogErr("accept failed! %d:%s", errno, QX_StrErr(errno));
        goto CommRet;
    }
    clientEvent = event_new(worker->EventBase, clientFd, EV_READ|EV_PERSIST, _ClientRecv, Arg);
    if (!clientEvent) {
        LogErr("new event failed!");
        goto CommRet;
    }
    event_add(clientEvent, NULL);
    clientNode = (QXS_CLIENT_NODE*)worker->Calloc(sizeof(QXS_CLIENT_NODE));
    if (!clientNode) {
        event_del(clientEvent);
        event_free(clientEvent);
        goto CommRet;
    }
    clientNode->Fd = clientFd;
    memcpy(&clientNode->ClientAddr, &clientAddr, sizeof(clientAddr));
    clientNode->RecvEvent = clientEvent;
    pthread_spin_lock(&worker->Lock);
    worker->ClientMap.insert(make_pair(clientFd, clientNode));
    pthread_spin_unlock(&worker->Lock);

CommRet:
    return ;
}

void*
QXServerWorker::_WorkerThreadFn(void *Arg) {
    QXServerWorker *worker = (QXServerWorker*)Arg;
    
    event_base_dispatch(worker->EventBase);
    // break at here
    event_base_free(worker->EventBase);
    worker->EventBase = NULL;

    return NULL;
}

QX_ERR_T QXServerWorker::InitEventBaseAndRun() {
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
    event_assign(&ListenEvent, EventBase, ServerFd, EV_READ|EV_PERSIST, _ServerAccept, (void*)this);
    event_add(&ListenEvent, NULL);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&ThreadId, &attr, _WorkerThreadFn, (void*)this);
    if (result != 0) {
        ret = -QX_ENOMEM;
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

QXServerWorker::QXServerWorker() {
    ServerFd = -1;
    Inited = FALSE;
    MemId = QX_UTIL_MEM_MODULE_INVALID_ID;
}

QXServerWorker::~QXServerWorker() {
    if (ServerFd != -1) {
        close(ServerFd);
    }
}

void* QXServerWorker::Calloc(size_t Size) {
    return QXUtil_MemCalloc(MemId, Size);
}

void QXServerWorker::Free(void* Ptr) {
    QXUtil_MemFree(MemId, Ptr);
}

QX_ERR_T QXServerWorker::InitClientMap(void) {
    pthread_spin_init(&Lock, PTHREAD_PROCESS_PRIVATE);

    return QX_SUCCESS;
}

QX_ERR_T QXServerWorker::Init(QX_SERVER_WORKER_INIT_PARAM InitParam) {
    QX_ERR_T ret = QX_SUCCESS;
    if (Inited) {
        goto CommonReturn;
    }
    // mem init
    ret = QXUtil_MemRegister(&MemId, (char*)InitParam.WorkerName.c_str());
    if (ret != QX_SUCCESS) {
        LogErr("Register mem module for %s failed!", InitParam.WorkerName.c_str());
        goto InitErr;
    }
    // base info init
    ServerName = InitParam.WorkerName;
    ret = InitServerFd(InitParam.PortRange, InitParam.WorkerLoad);
    if (ret != QX_SUCCESS) {
        LogErr("Init worker for %s failed!", InitParam.WorkerName.c_str());
        goto InitErr;
    }
    // init Client Map 
    ret = InitClientMap();
    if (ret != QX_SUCCESS) {
        LogErr("Init client map for %s failed! ret %d", InitParam.WorkerName.c_str(), ret);
        goto InitErr;
    }
    // init EventBase And start thread
    ret = InitEventBaseAndRun();
    if (ret != QX_SUCCESS) {
        LogErr("Init event base for %s failed! ret %d", InitParam.WorkerName.c_str(), ret);
        goto InitErr;
    }
    
    Inited = TRUE;
    goto CommonReturn;

InitErr:
    Exit();
CommonReturn:
    return ret;
}

void QXServerWorker::Exit() {
    if (Inited) {
        if (ServerFd != -1) {
            close(ServerFd);
        }
        if (EventBase) {
            event_base_loopbreak(EventBase);
        }
        pthread_spin_destroy(&Lock);
        QXUtil_MemUnRegister(&MemId);
        Inited = FALSE;
        LogDbg("%s exited...", ServerName.c_str());
    }
    return ;
}

string QXServerWorker::GetStatus(void) {
    string statsBuff;

    pthread_spin_lock(&Lock);
    statsBuff += "ServerWorker:" + ServerName + '\n';
    for (map<int, QXS_CLIENT_NODE*>::iterator it = ClientMap.begin(); it != ClientMap.end(); it ++) {
        statsBuff += "Fd:" + to_string(it->first) + " " + "ClientId:" + to_string(it->second->ClientId) + " ";
        statsBuff += string("Ip:") + inet_ntoa(it->second->ClientAddr.sin_addr) +"\n";
    }
    pthread_spin_unlock(&Lock);

    return statsBuff;
}