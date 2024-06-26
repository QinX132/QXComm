#include <curl/curl.h>
#include <filesystem>

#include "QXSCMsg.pb.h"
#include "QXClientWorker.h"
#include "QXUtilsCommonUtil.h"
#include "QXClientMsgHandler.h"

using namespace std;

static string sg_QXCStatString[QXC_WORKER_STATS_MAX] = {
    [QXC_WORKER_STATS_UNSPEC]          =   "Unknown",
    [QXC_WORKER_STATS_INITED]          =   "Inited",
    [QXC_WORKER_STATS_CONNECTED]       =   "Connected",
    [QXC_WORKER_STATS_REGISTERED]      =   "Active",
    [QXC_WORKER_STATS_DISCONNECTED]    =   "Disconnected",
    [QXC_WORKER_STATS_EXIT]            =   "Exited",
};

#define QX_CLIENT_WORKER_PUBKEY_FILE                            "QXC_SM2_Pub.pem"
#define QX_CLIENT_WORKER_PRIKEY_FILE                            "QXC_SM2_Pri.pem"

QX_ERR_T 
QXClientWorker::InitPath(std::string WorkPath) {
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
QXClientWorker::InitSm2Key(std::string CryptoPath, std::string PriKeyPwd) {
    QX_ERR_T ret = QX_SUCCESS;
    std::filesystem::path cryptoDir = CryptoPath;
    std::filesystem::path pubkeyFile = cryptoDir / QX_CLIENT_WORKER_PUBKEY_FILE;
    std::filesystem::path prikeyFile = cryptoDir / QX_CLIENT_WORKER_PRIKEY_FILE;
    
    try {
        if (!std::filesystem::exists(pubkeyFile) || !std::filesystem::exists(prikeyFile)) {
            ret = QXUtil_CryptSm2KeyGenAndExport(pubkeyFile.string().c_str(), prikeyFile.string().c_str(), PriKeyPwd.c_str());
            if (ret) {
                LogErr("GenKey failed! ret %d", ret);
                return ret;
            } else {
                LogDbg("Genkey success!");
                PriKeyPath = prikeyFile.string();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to check file existence. %s", e.what());
        return -QX_EIO;
    }

    ret = QXUtil_CryptSm2ImportPubKey(pubkeyFile.string().c_str(), &PubKey);
    if (ret) {
        LogErr("Import key failed! ret %d", ret);
        return ret;
    }
    QXClientWorker::PriKeyPath = prikeyFile.string();
    QXUtil_Hexdump("Local-Sm2PubKey", (uint8_t*)&PubKey.public_key, sizeof(PubKey.public_key));

    return ret;
}

QX_ERR_T QXClientWorker::GetServerInfo(void) {
    QX_ERR_T ret = QX_SUCCESS;
    ret =  QXUtil_CryptSm2ImportPubKey("worker1/QXS_SM2_Pub.pem", &RegisterCtx.PubKey);
    
    QXUtil_Hexdump("Server-Sm2PubKey", (uint8_t*)&RegisterCtx.PubKey.public_key, sizeof(RegisterCtx.PubKey.public_key));
    return ret;
}

QX_ERR_T QXClientWorker::ConnectServer(void) {
    QX_ERR_T ret = QX_SUCCESS;
    bool connectSuccess = false;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    ret = GetServerInfo();
    if (ret) {
        LogErr("Get ServerInfo failed!");
        goto CommRet;
    }
    for(//size_t loop = CurrentServerPos >= 0 ? CurrentServerPos + 1 : rand() % InitParam.Servers.size(); 
        size_t loop = 0;
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
        RegisterCtx.Status = 0;
        RegisterCtx.RegisterRetried = 0;
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
    MsgHandler->ProtoInitMsg(msgPayload);
    // check register send
    if (RegisterCtx.RegisterRetried >= 10) {
        RemoveRecvEvent();
        RecreateWorkerFd();
        State = QXC_WORKER_STATS_DISCONNECTED;
        LogErr("Retried too much, set dissconenct.");
        goto CommRet;
    }
    // create registerproto
    ret = MsgHandler->CreateRegisterProtoMsg(*msgPayload.mutable_msgbase());
    if (ret) {
        ret = -QX_EINVAL;
        LogErr("Create proto register failed!");
        goto CommRet;
    }
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
    RegisterCtx.RegisterRetried ++;

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
    uint8_t *plain = NULL;
    size_t plainLen = 0;
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
    if (worker->RegisterCtx.Status == QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH) {
        if (worker->RegisterCtx.TransCipherSuite == QXSCMsg::QX_SC_CIPHER_SUITE_SM4) {
            plainLen = recvMsg->Head.ContentLen;
            plain = new uint8_t[plainLen];
            ret = QXUtil_CryptSm4CBCDecrypt(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen - QXUTIL_CRYPT_SM4_IV_LEN,
                    worker->RegisterCtx.TransKey.Sm4Key, sizeof(worker->RegisterCtx.TransKey.Sm4Key),
                    recvMsg->Cont.VarLenCont + recvMsg->Head.ContentLen - QXUTIL_CRYPT_SM4_IV_LEN, QXUTIL_CRYPT_SM4_IV_LEN,
                    plain, &plainLen);
            if (ret) {
                LogErr("Decrypt failed! ret %d", ret);
                goto CommRet;
            }
            recvMsg->Head.ContentLen = plainLen;
            memcpy(recvMsg->Cont.VarLenCont, plain, plainLen);
        }
    }

    if (!msgPayload.ParseFromArray(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
        LogErr("parse form array failed!");
        goto CommRet;
    }
    LogInfo("Recv msg: %s", msgPayload.ShortDebugString().c_str());
    ret = worker->MsgHandler->DispatchMsg(msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (recvMsg)
        QXUtil_FreeRecvQMsg(recvMsg);
    if (plain)
        delete plain;
    return ;
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

void QXClientWorker::_StateMachineFn(
    evutil_socket_t Fd,
    short Ev, 
    void* Arg
    )
{
    QXClientWorker *worker = (QXClientWorker*)Arg;
    QX_ERR_T ret = QX_SUCCESS;
    UNUSED(Ev);
    UNUSED(Fd);
    
    switch (worker->State) {
        case QXC_WORKER_STATS_INITED:
        case QXC_WORKER_STATS_DISCONNECTED:
            ret = worker->ConnectServer();
            if (ret < QX_SUCCESS) {
                LogErr("Connect failed! ret %d", ret);
                goto CommRet;
            }
            ret = worker->RegisterToServer();
            if (ret < QX_SUCCESS) {
                LogErr("Send register msg failed! ret %d", ret);
                goto CommRet;
            }
            break;
        case QXC_WORKER_STATS_CONNECTED:
            ret = worker->RegisterToServer();
            if (ret < QX_SUCCESS) {
                LogErr("Send register msg failed! ret %d", ret);
                goto CommRet;
            }
            break;
        case QXC_WORKER_STATS_REGISTERED:
            break;
        case QXC_WORKER_STATS_EXIT:
        default:
            LogErr("In stat %d, exiting stat machine!", worker->State);
            QXUtil_TimerDel(&worker->StatMachineTimer);
            worker->StatMachineTimer = NULL;
            goto CommRet;
    }
    if (worker->State != QXC_WORKER_STATS_REGISTERED)
        LogInfo("Worker in stat <%s>.", sg_QXCStatString[worker->State].c_str());

CommRet:
    return ;
}

QX_ERR_T QXClientWorker::StartStateMachine(void) {
    QX_ERR_T ret = QX_SUCCESS;

    ret = QXUtil_TimerAdd(_StateMachineFn, 3 * 1000, (void*)this, QX_UTIL_TIMER_TYPE_LOOP, TRUE, &StatMachineTimer);
    if (ret) {
        LogErr("Add timer! ret %d\n", ret);
    }

    return ret;
}

QX_ERR_T QXClientWorker::Init(QXC_WORKER_INIT_PARAM InitParam){
    QX_ERR_T ret = QX_SUCCESS;

    if (QXC_IS_INITED(State)) {
        goto CommRet;
    }
    
    QXClientWorker::InitParam = InitParam;
    
    // workPath init
    ret = InitPath(InitParam.WorkPath);
    if (ret != QX_SUCCESS) {
        LogErr("Init path for %d failed! ret %d", InitParam.ClientId, ret);
        goto InitErr;
    }
    // Sm2 init
    ret = InitSm2Key(InitParam.WorkPath, InitParam.PriKeyPwd);
    if (ret != QX_SUCCESS) {
        LogErr("Init sm2 key for %d failed! ret %d", InitParam.ClientId, ret);
        goto InitErr;
    }

    MsgHandler = new QXClientMsgHandler(this);
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
        if (StatMachineTimer) {
            QXUtil_TimerDel(&StatMachineTimer);
            StatMachineTimer = NULL;
        }
        State = QXC_WORKER_STATS_EXIT;
    }
}

QX_ERR_T
QXClientWorker::SendMsg(uint32_t ClientId, std::string Msg) {
    QXSCMsg::MsgPayload sendMsgPayload;
    QX_ERR_T ret = QX_SUCCESS;
    std::string serializedData;
    const uint8_t* plain = NULL;
    size_t plainLen = 0;
    uint8_t *cipher = NULL;
    size_t cipherLen = 0;
    uint8_t iv[QXUTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);
    QX_UTIL_Q_MSG *sendMsg = NULL;
    
    if (RegisterCtx.Status != QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH) {
        std::cout << "Not registered, please wait or check" << '\n';
        goto CommRet;
    }

    MsgHandler->ProtoInitMsg(sendMsgPayload);
    sendMsgPayload.mutable_msgbase()->set_msgtype(QXSCMsg::QX_SC_MSG_TYPE_MSG_TRANS_C_2_S);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(InitParam.ClientId);
    // to my self
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(ClientId);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg(Msg);
    // pre send
    MsgHandler->ProtoPreSend(sendMsgPayload);
    serializedData = sendMsgPayload.SerializeAsString();
    // encrypt
    if (RegisterCtx.Status == QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH &&
        RegisterCtx.TransCipherSuite == QXSCMsg::QX_SC_CIPHER_SUITE_SM4) {
        cipherLen = QXUtil_CryptSm4CBCGetPaddedLen(serializedData.size()) + sizeof(iv);
        cipher = (uint8_t*)malloc(cipherLen);
        if (!cipher) {
            goto CommRet;
        }
        plain = reinterpret_cast<const uint8_t*>(serializedData.c_str());
        plainLen = serializedData.size();
        ret = QXUtil_CryptSm4CBCEncrypt(plain, plainLen, RegisterCtx.TransKey.Sm4Key, sizeof(RegisterCtx.TransKey.Sm4Key), cipher, &cipherLen, iv, &ivLen);
        if (ret || ivLen != sizeof(iv)) {
            LogErr("encrypt msg failed!");
            goto CommRet;
        }
        memcpy(cipher + cipherLen, iv, ivLen);
        cipherLen += ivLen;
        sendMsg = QXUtil_NewSendQMsg(cipherLen);
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, cipher, cipherLen);
    }
    ret = QXUtil_SendQMsg(WorkerFd, sendMsg);
    if (ret < QX_SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", sendMsgPayload.ShortDebugString().c_str());

CommRet:
    if (cipher)
        free(cipher);
    if (sendMsg) 
        QXUtil_FreeSendQMsg(sendMsg);
    return ret;
}

QXClientWorker::QXClientWorker() :
    WorkerFd(-1), State(QXC_WORKER_STATS_UNSPEC), StatMachineTimer(NULL), EventBase(NULL), RecvEvent(NULL), MsgHandler(NULL), CurrentServerPos(-1){}

QXClientWorker::~QXClientWorker() {}
