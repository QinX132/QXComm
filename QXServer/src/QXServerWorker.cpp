#include <filesystem>

#include "QXSCMsg.pb.h"
#include "QXServerWorker.h"

using namespace std;

#undef LogInfo
#undef LogDbg
#undef LogWarn
#undef LogErr
#define LogInfo(FMT, ...)       LogClassInfo("QXSvrWrkr", FMT, ##__VA_ARGS__)
#define LogDbg(FMT, ...)        LogClassInfo("QXSvrWrkr", FMT, ##__VA_ARGS__)
#define LogWarn(FMT, ...)       LogClassInfo("QXSvrWrkr", FMT, ##__VA_ARGS__)
#define LogErr(FMT, ...)        LogClassInfo("QXSvrWrkr", FMT, ##__VA_ARGS__)

#define QX_SERVER_WORKER_PUBKEY_FILE                            "QXS_SM2_Pub.pem"
#define QX_SERVER_WORKER_PRIKEY_FILE                            "QXS_SM2_Pri.pem"

QX_ERR_T 
QXServerWorker::InitPath(std::string WorkPath) {
    QX_ERR_T ret = QX_SUCCESS;
    std::filesystem::path workDir = WorkPath;
    
    try {
        if (!std::filesystem::exists(workDir)) {
            LogInfo("Directory does not exist, creating...");
            if (std::filesystem::create_directories(workDir)) {
                LogInfo("Directory created successfully.");
            } else {
                LogErr("Failed to create directory.");
                return -QX_EIO;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to create directory. %s", e.what());
        return -QX_EIO;
    }

    return ret;
}

QX_ERR_T 
QXServerWorker::InitSm2Key(std::string CryptoPath, std::string PriKeyPwd) {
    QX_ERR_T ret = QX_SUCCESS;
    std::filesystem::path cryptoDir = CryptoPath;
    std::filesystem::path pubkeyFile = cryptoDir / QX_SERVER_WORKER_PUBKEY_FILE;
    std::filesystem::path prikeyFile = cryptoDir / QX_SERVER_WORKER_PRIKEY_FILE;
    
    try {
        if (!std::filesystem::exists(pubkeyFile) || !std::filesystem::exists(prikeyFile)) {
            ret = QXUtil_CryptSm2KeyGenAndExport(pubkeyFile.string().c_str(), prikeyFile.string().c_str(), PriKeyPwd.c_str());
            if (ret) {
                LogErr("GenKey failed! ret %d", ret);
                return ret;
            } else {
                LogDbg("Genkey success!");
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to check file existence. %s", e.what());
        return -QX_EIO;
    }
    QXServerWorker::Sm2PriKeyPath = prikeyFile.string();

    ret = QXUtil_CryptSm2ImportPubKey(pubkeyFile.string().c_str(), &Sm2PubKey);
    if (ret) {
        LogErr("Import key failed! ret %d", ret);
        return ret;
    }
    QXUtil_Hexdump("Local-Sm2PubKey", (uint8_t*)&Sm2PubKey.public_key, sizeof(Sm2PubKey.public_key));

    return ret;
}

QX_ERR_T
QXServerWorker::InitWorkerFd(uint16_t Port, uint32_t Load) {
    struct sockaddr_in serverAddr;
    QX_ERR_T ret = QX_SUCCESS;
    int option = 1, flags = 0;
    // socket
    WorkerFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WorkerFd < 0) {
        LogErr("Socket failed!");
        ret = -QX_EBADFD; 
        goto CreateFdFail;
    }
    // reuse && nonblock
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags = fcntl(WorkerFd, F_GETFL, 0);
    if (flags == -1) {
        LogErr("get flags failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags |= O_NONBLOCK;
    if (fcntl(WorkerFd, F_SETFL, flags) == -1) {
        LogErr("set nonblock failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    // bind port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(Port);
    if (bind(WorkerFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        ret = -QX_EBADF; 
        goto CommErr;
    }
    // listen
    if (listen(WorkerFd, Load) == -1) {
        LogErr("listen failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }

    LogInfo("Create server worker, fd %d, loading %u.", WorkerFd, Load);
    goto Success;
    
CommErr:
    close(WorkerFd);
    WorkerFd = -1;
CreateFdFail:
Success:
    return ret;
}

void QXServerWorker::EraseClient_NL(int32_t Fd) {
    auto it = ClientMap.find(Fd);
    if (it != ClientMap.end()) {
        QXS_CLIENT_NODE* clientNode = it->second;
        if (clientNode) {
            LogWarn("[%s] Erasing client %u, Fd %d.", InitParam.Name.c_str(), clientNode->ClientId, Fd);
            if (clientNode->RecvEvent) {
                epoll_ctl(EpollFd, EPOLL_CTL_DEL, Fd, NULL);
                Free(clientNode->RecvEvent);
                clientNode->RecvEvent = NULL;
            }
            if (clientNode->RegisterCtx.Status == QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH && ClientCurrentNum.load() > 0) {
                clientNode->RegisterCtx.Status = 0;
                ClientCurrentNum.fetch_sub(1);
            }
            if (clientNode->Fd >= 0) {
                close(clientNode->Fd);
            }
            Free(clientNode);
            auto iter = ClientFdMap.find(clientNode->ClientId);
            if (iter != ClientFdMap.end()) {
                ClientFdMap.erase(clientNode->ClientId);
            }
        }
        ClientMap.erase(it);
    }
}

void QXServerWorker::EraseClient(int32_t Fd) {
    pthread_spin_lock(&Lock);
    EraseClient_NL(Fd);
    pthread_spin_unlock(&Lock);
}

void QXServerWorker::ClientRecv(int32_t Fd) {
    QX_ERR_T ret = QX_SUCCESS;
    QX_UTIL_Q_MSG *recvMsg = NULL;
    QXSCMsg::MsgPayload msgPayload;
    uint8_t *msgPlain = NULL;
    size_t msgPlainLen = 0;

    recvMsg = QXUtil_NewRecvQMsg();
    if (!recvMsg) {
        LogErr("Apply failed!");
        return ;
    }
    ret = QXUtil_RecvQMsg(Fd, recvMsg);
    if (ret < 0) {
        EraseClient(Fd);
        LogErr("recv failed!");
        goto CommRet;
    }
    if (ClientMap[Fd]->RegisterCtx.Status != QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH) {
        // no need to decrypt
        if (!msgPayload.ParseFromArray(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
            LogErr("parse form array failed!");
            goto CommRet;
        }
    } else if (ClientMap[Fd]->RegisterCtx.TransCipherSuite == QXSCMsg::QX_SC_CIPHER_SUITE_SM4){
        // decyrpt
        msgPlainLen = recvMsg->Head.ContentLen;
        msgPlain = (uint8_t*)Calloc(msgPlainLen);
        if (!msgPlain) {
            goto CommRet;
        }
        ret = QXUtil_CryptSm4CBCDecrypt(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen - QXUTIL_CRYPT_SM4_IV_LEN, 
            ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key, sizeof(ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key), 
            recvMsg->Cont.VarLenCont + recvMsg->Head.ContentLen - QXUTIL_CRYPT_SM4_IV_LEN, QXUTIL_CRYPT_SM4_IV_LEN, msgPlain, &msgPlainLen);
        if (ret) {
            LogErr("decrypt msg failed!");
            goto CommRet;
        }
        if (!msgPayload.ParseFromArray(msgPlain, msgPlainLen)) {
            LogErr("parse form array failed!");
            goto CommRet;
        }
    } else {
        ret = -QX_EINVAL;
        LogWarn("Ignore msg!");
        goto CommRet;
    }
    LogInfo("[%s]recv msg: %s", InitParam.Name.c_str(), msgPayload.ShortDebugString().c_str());
    ret = MsgHandler->DispatchMsg(Fd, msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (recvMsg)
        QXUtil_FreeRecvQMsg(recvMsg);
    if (msgPlain) 
        Free(msgPlain);
    return ;
}

void
QXServerWorker::ServerAccept(void) {
    struct sockaddr_in clientAddr;
    socklen_t len = 0;
    int clientFd = -1;
    QXS_CLIENT_NODE *clientNode = NULL;
    struct timeval tv;
    QX_ERR_T ret = QX_SUCCESS;

    clientFd = accept(WorkerFd, (struct sockaddr*)&clientAddr, &len);
    if (clientFd < 0) {
        ret = -QX_EIO;
        LogErr("accept failed! %d:%s", errno, QX_StrErr(errno));
        goto CommRet;
    }
    LogInfo("[%s] %s connect.", InitParam.Name.c_str(), inet_ntoa(clientAddr.sin_addr));
    // timeout
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -QX_EIO;
        LogErr("Set recv timeout failed\n");
        goto CommRet;
    }
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -QX_EIO;
        LogErr("Set send timeout failed\n");
        goto CommRet;
    }
    clientNode = (QXS_CLIENT_NODE*)Calloc(sizeof(QXS_CLIENT_NODE));
    if (!clientNode) {
        ret = -QX_ENOMEM;
        LogErr("new client node failed!");;
        goto CommRet;
    }
    clientNode->RecvEvent = (struct epoll_event*)Calloc(sizeof(struct epoll_event));
    if (!clientNode->RecvEvent) {
        ret = -QX_ENOMEM;
        LogErr("new event failed!");
        goto CommRet;
    }
    clientNode->Fd = clientFd;
    clientNode->RecvEvent->events = EPOLLIN;        //we do not use EPOLLET here
    clientNode->RecvEvent->data.fd = clientFd;
    clientNode->RegisterCtx.Status = 0;
    epoll_ctl(EpollFd, EPOLL_CTL_ADD, clientFd, clientNode->RecvEvent);
    
    pthread_spin_lock(&Lock);
    ClientMap.insert(make_pair(clientFd, clientNode));
    pthread_spin_unlock(&Lock);

CommRet:
    if (ret < QX_SUCCESS) {
        if (clientFd >= 0)
            close (clientFd);
        if (clientNode && !clientNode->RecvEvent) {
            Free(clientNode);
        }
    }
    return ;
}

#define QXS_WAIT_MAX_EVENTS                             256
#define QXS_WAIT_INTERVAL                               100 // ms

void*
QXServerWorker::_WorkerThreadFn(void *Arg) {
    QXServerWorker *worker = (QXServerWorker*)Arg;
    struct epoll_event events[QXS_WAIT_MAX_EVENTS];
    int nfds = 0;
    
    while (worker->Inited) {
        nfds = epoll_wait(worker->EpollFd, events, QXS_WAIT_MAX_EVENTS, QXS_WAIT_INTERVAL);
        if (nfds < 0) {
            LogErr("epoll_wait err! %d:%s", errno, strerror(errno));
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == worker->WorkerFd) {
                worker->ServerAccept();
            } else if (events[i].events & EPOLLIN) {
                worker->ClientRecv(events[i].data.fd);
            } else if ( events[i].events & EPOLLERR || 
                        events[i].events & EPOLLRDHUP ||
                        events[i].events & EPOLLHUP) {
                worker->EraseClient(events[i].data.fd);
            }
        }
    }

    return NULL;
}

QX_ERR_T QXServerWorker::InitEventBaseAndRun() {
    QX_ERR_T ret = QX_SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;

    EpollFd = epoll_create1(0);
    if (EpollFd < 0) {
        LogErr("create epoll fd failed!\n");
        goto NewEventBaseErr;
    }
    ListenEvent.events = EPOLLIN | EPOLLET;
    ListenEvent.data.fd = WorkerFd;
    epoll_ctl(EpollFd, EPOLL_CTL_ADD, WorkerFd, &ListenEvent);

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
    EpollFd = -1;
CommRet:
    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

QXServerWorker::QXServerWorker() {
    WorkerFd = -1;
    Inited = FALSE;
    MemId = QX_UTIL_MEM_MODULE_INVALID_ID;
    EpollFd = -1;
    MsgHandler = NULL;
    ClientCurrentNum.store(0);
}

QXServerWorker::~QXServerWorker() {
    if (WorkerFd != -1) {
        close(WorkerFd);
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

QX_ERR_T QXServerWorker::Init(QXS_WORKER_INIT_PARAM InInitParam) {
    QX_ERR_T ret = QX_SUCCESS;
    if (Inited) {
        goto CommonReturn;
    }
    Inited = TRUE;
    InitParam = InInitParam;
    MsgHandler = new QXServerMsgHandler(this);
    if (!MsgHandler) {
        LogErr("New msghandler for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // workPath init
    ret = InitPath(InitParam.WorkPath);
    if (ret != QX_SUCCESS) {
        LogErr("Init path for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // Sm2 init
    ret = InitSm2Key(InitParam.WorkPath, InitParam.Sm2PriKeyPwd);
    if (ret != QX_SUCCESS) {
        LogErr("Init sm2 key for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // mem init
    ret = QXUtil_MemRegister(&MemId, (char*)InitParam.Name.c_str());
    if (ret != QX_SUCCESS) {
        LogErr("Register mem module for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // socket init
    ret = InitWorkerFd(InitParam.Port, InitParam.Load);
    if (ret != QX_SUCCESS) {
        LogErr("Init worker for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // init Client Map 
    ret = InitClientMap();
    if (ret != QX_SUCCESS) {
        LogErr("Init client map for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // init EventBase And start thread
    ret = InitEventBaseAndRun();
    if (ret != QX_SUCCESS) {
        LogErr("Init event base for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    
    goto CommonReturn;

InitErr:
    Exit();
CommonReturn:
    return ret;
}

void QXServerWorker::Exit() {
    if (Inited) {
        if (WorkerFd != -1) {
            close(WorkerFd);
            WorkerFd = -1;
        }
        if (EpollFd >= 0) {
            close(EpollFd);
            EpollFd = -1;
        }
        if (MsgHandler) {
            delete MsgHandler;
            MsgHandler = NULL;
        }
        pthread_spin_lock(&Lock);
        for (auto it = ClientMap.begin(); it != ClientMap.end(); ) {
            QXS_CLIENT_NODE* clientNode = it->second;
            if (clientNode) {
                LogWarn("[%s] Erasing client %u, Fd %d.", 
                    InitParam.Name.c_str(), clientNode->ClientId, clientNode->Fd);
                if (clientNode->RecvEvent) {
                    Free(clientNode->RecvEvent);
                    clientNode->RecvEvent = NULL;
                }
                clientNode->RegisterCtx.Status = 0;
                if (clientNode->Fd >= 0) {
                    close(clientNode->Fd);
                }
                Free(clientNode);
            }
            it = ClientMap.erase(it);
        }
        for (auto it = ClientFdMap.begin(); it != ClientFdMap.end(); ) {
            it = ClientFdMap.erase(it);
        }
        pthread_spin_unlock(&Lock);
        pthread_spin_destroy(&Lock);
        QXUtil_MemUnRegister(&MemId);
        LogDbg("%s exited.", InitParam.Name.c_str());
        Inited = FALSE;
    }
    return ;
}

string QXServerWorker::GetStatus(void) {
    string statsBuff;
    int cnt = 0;

    statsBuff += "ServerWorker:" + InitParam.Name + " CurrentClientNum:" + to_string(ClientCurrentNum.load())+ '\n';
    pthread_spin_lock(&Lock);
    for (auto it = ClientMap.begin(); it != ClientMap.end(); it ++) {
        statsBuff += "[Fd:" + to_string(it->second->Fd) + " " + "ClientId:" + to_string(it->second->ClientId) + " ";
        if (++cnt % 4 == 0)
            statsBuff += "\n";
    }
    pthread_spin_unlock(&Lock);
    statsBuff += "\n";
    
    return statsBuff;
}