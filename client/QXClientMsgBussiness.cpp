#include <chrono>

#include "QXClientMsgBussiness.h"
#include "QXClientWorker.h"

using namespace QXSCMsg;

void QXClientMsgHandler::ProtoInitMsg(
    QXClientWorker* Worker,
    MsgPayload &MsgPayload
    )
{
    assert(NULL != Worker);
    assert(NULL != MsgPayload.mutable_msgbase());
    assert(NULL != MsgPayload.mutable_clientinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_clientinfo()->set_clientid(Worker->InitParam.ClientId);
}

void QXClientMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.set_timestamp(timestamp);
    TransId.fetch_add(1);
}

void QXClientMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    assert(NULL != MsgPayload.release_msgbase());
    assert(NULL != MsgPayload.release_clientinfo());
}

void QXClientMsgHandler::CreateRegisterProtoMsg(
    QXClientWorker* Worker,
    MsgBase &MsgBase
    ) 
{
    MsgBase.set_msgtype(QX_MSG_TYPE_REGISTER);
    MsgBase.mutable_clientregister()->set_clientid(Worker->InitParam.ClientId);
}

QX_ERR_T QXClientMsgHandler::DispatchMsg(QXClientWorker* Worker, MsgPayload MsgPayload) {
    switch (MsgPayload.msgbase().msgtype()) {
        case QX_MSG_TYPE_REGISTER_REPLY:
            if (MsgPayload.msgbase().clientregisterreply().errcode() == QX_SUCCESS)
                    Worker->State = QXC_WORKER_STATS_REGISTERED;
            else 
                LogErr("Register replies as fail! errcode %d", MsgPayload.msgbase().clientregisterreply().errcode());
            break;
        default:
            break;
    }

    return QX_SUCCESS;
}

QXClientMsgHandler::QXClientMsgHandler():TransId(0){
    TransId.fetch_add(1);
};
QXClientMsgHandler::~QXClientMsgHandler(){};
