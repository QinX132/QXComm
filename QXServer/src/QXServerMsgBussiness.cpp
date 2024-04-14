#include <chrono>

#include "QXServerMsgBussiness.h"
#include "QXServerWorker.h"

using namespace QXSCMsg;

#undef LogInfo
#undef LogDbg
#undef LogWarn
#undef LogErr
#define LogInfo(FMT, ...)       LogClassInfo("QXSMsgHdlr", FMT, ##__VA_ARGS__)
#define LogDbg(FMT, ...)        LogClassInfo("QXSMsgHdlr", FMT, ##__VA_ARGS__)
#define LogWarn(FMT, ...)       LogClassInfo("QXSMsgHdlr", FMT, ##__VA_ARGS__)
#define LogErr(FMT, ...)        LogClassInfo("QXSMsgHdlr", FMT, ##__VA_ARGS__)

typedef struct _QXS_TPOOL_ARGS{
    MsgPayload *SendMsg;
    int32_t Fd;
    QXServerWorker* Worker;
    QXServerMsgHandler* MsgHandler;
}
QXS_TPOOL_ARG;

extern "C" void QXServer_TPoolFunc(void* Arg) {
    QXS_TPOOL_ARG *arg = (QXS_TPOOL_ARG*)Arg;
    QX_UTIL_Q_MSG *sendMsg = NULL;
    std::string serializedData;
    QX_ERR_T ret = QX_SUCCESS;

    if (!arg || !arg->MsgHandler || !arg->SendMsg || !arg->Worker) {
        goto CommRet;
    }

    arg->MsgHandler->ProtoPreSend(*arg->SendMsg);
    serializedData = arg->SendMsg->SerializeAsString();
    sendMsg = QXUtil_NewSendQMsg(serializedData.size());
    if (!sendMsg) {
        goto CommRet;
    }
    memcpy(sendMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    ret = QXUtil_SendQMsg(arg->Fd, sendMsg);
    if (ret < QX_SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", arg->SendMsg->ShortDebugString().c_str());

CommRet:
    if (sendMsg) 
        QXUtil_FreeSendQMsg(sendMsg);
    if (arg->SendMsg && arg->MsgHandler) {
        arg->MsgHandler->ProtoRelease(*arg->SendMsg);
        delete arg->SendMsg;
        arg->SendMsg = NULL;
    }
    if (arg && arg->Worker)
        arg->Worker->Free(arg);
    return ;
}

void QXServerMsgHandler::ProtoInitMsg(
    QXServerWorker* Worker,
    MsgPayload &MsgPayload
    )
{
    assert(NULL != Worker);
    assert(NULL != MsgPayload.mutable_msgbase());
    assert(NULL != MsgPayload.mutable_serverinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_serverinfo()->set_servername(Worker->InitParam.WorkerName);
}

void QXServerMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.set_timestamp(timestamp);
    TransId.fetch_add(1);
}

void QXServerMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    assert(NULL != MsgPayload.release_msgbase());
    assert(NULL != MsgPayload.release_serverinfo());
}

QX_ERR_T QXServerMsgHandler::RegisterClient(
    int32_t Fd,
    QXServerWorker* Worker,
    const MsgPayload MsgPayload
    )
{
    QXS_TPOOL_ARG *tpoolArg = NULL;
    QX_ERR_T ret = QX_SUCCESS;
    
    pthread_spin_lock(&Worker->Lock);
    if (Worker->ClientMap.count(Fd) > 0) {
        Worker->ClientMap[Fd]->Registered = true;
        Worker->ClientMap[Fd]->ClientId = MsgPayload.msgbase().clientregister().clientid();
    } else {
        ret = -QX_EEXIST;
        pthread_spin_unlock(&Worker->Lock);
        goto CommRet;
    }
    pthread_spin_unlock(&Worker->Lock);
    Worker->ClientCurrentNum.fetch_add(1);
    
    tpoolArg = (QXS_TPOOL_ARG*)Worker->Calloc(sizeof(QXS_TPOOL_ARG));
    if (!tpoolArg) {
        ret = -QX_ENOMEM;
        goto CommRet;
    }
    tpoolArg->SendMsg = new QXSCMsg::MsgPayload;
    ProtoInitMsg(Worker, *tpoolArg->SendMsg);
    tpoolArg->SendMsg->mutable_msgbase()->set_msgtype(QX_SC_MSG_TYPE_REGISTER_REPLY);
    tpoolArg->SendMsg->set_errcode(ret);
    tpoolArg->SendMsg->mutable_msgbase()->mutable_clientregisterreply()->set_errcode(ret);
//    if (ret == QX_SUCCESS)
//        tpoolArg->SendMsg->mutable_msgbase()->mutable_clientregisterreply()->clear_errcode(); // 0 has to be cleared
    
    tpoolArg->Fd = Fd;
    tpoolArg->MsgHandler = this;
    tpoolArg->Worker = Worker;
    ret = QXUtil_TPoolAddTask(QXServer_TPoolFunc, (void*)tpoolArg);
    if (ret < QX_SUCCESS) {
        ProtoRelease(*tpoolArg->SendMsg);
        delete tpoolArg->SendMsg;
        tpoolArg->SendMsg = NULL;
        Worker->Free(tpoolArg);
    }

CommRet:
    return ret;
}

QX_ERR_T QXServerMsgHandler::DispatchMsg(
    int32_t Fd,
    QXServerWorker* Worker,
    const MsgPayload MsgPayload
    ) 
{
    QX_ERR_T ret = QX_SUCCESS;
    switch (MsgPayload.msgbase().msgtype()) {
        case QX_SC_MSG_TYPE_REGISTER:
            if (Worker->ClientCurrentNum.load() < Worker->InitParam.WorkerLoad) {
                ret = RegisterClient(Fd, Worker, MsgPayload);
            } else {
                LogWarn("ClientNum has reached its upper limit %u, ignore connect request!", Worker->ClientCurrentNum.load());
                ret = -QX_EBUSY;
            }
            break;
        default:
            LogErr("Invalid type:%d", MsgPayload.msgbase().msgtype());
            return -QX_EIO;
    }
    return ret;
}

QXServerMsgHandler::QXServerMsgHandler():TransId(0){
    TransId.fetch_add(1);
};
QXServerMsgHandler::~QXServerMsgHandler(){};

