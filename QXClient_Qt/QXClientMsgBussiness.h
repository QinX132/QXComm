#ifndef _QX_CLIENT_MSG_BUSSINESS_H_
#define _QX_CLIENT_MSG_BUSSINESS_H_

#include <atomic>
#include "QXSCMsg.pb.h"
#include "QXUtilsCommonUtil.h"

class QXClientWorker;

class QXClientMsgHandler{
private:
    std::atomic<uint32_t> TransId;
public:
    QXClientMsgHandler();
    ~QXClientMsgHandler();
    void ProtoInitMsg(QXClientWorker*, QXSCMsg::MsgPayload&);
    void ProtoPreSend(QXSCMsg::MsgPayload&);
    void ProtoRelease(QXSCMsg::MsgPayload&);
    QX_ERR_T DispatchMsg(QXClientWorker*, QXSCMsg::MsgPayload);
    void CreateRegisterProtoMsg(QXClientWorker*, QXSCMsg::MsgBase&);
};

#endif /* _QX_CLIENT_MSG_BUSSINESS_H_ */
