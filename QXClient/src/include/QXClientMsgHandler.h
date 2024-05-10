#ifndef _QX_CLIENT_MSG_HANDLER_H_
#define _QX_CLIENT_MSG_HANDLER_H_

#include <atomic>
#include "QXSCMsg.pb.h"
#include "QXUtilsCommonUtil.h"

class QXClientWorker;

class QXClientMsgHandler{
private:
    std::atomic<uint32_t> TransId;
    QXClientWorker* ClientWorker;
public:
    QXClientMsgHandler(QXClientWorker*);
    ~QXClientMsgHandler();
    void ProtoInitMsg(QXSCMsg::MsgPayload&);
    void ProtoPreSend(QXSCMsg::MsgPayload&);
    void ProtoRelease(QXSCMsg::MsgPayload&);
    QX_ERR_T DispatchMsg(QXSCMsg::MsgPayload);
    QX_ERR_T CreateRegisterProtoMsg(QXSCMsg::MsgBase&);
};

#endif /* _QX_CLIENT_MSG_HANDLER_H_ */
