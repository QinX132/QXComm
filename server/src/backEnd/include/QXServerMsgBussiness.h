#ifndef _QX_SERVER_MSG_BUSSINESS_H_
#define _QX_SERVER_MSG_BUSSINESS_H_

#include <atomic>
#include "QXSCMsg.pb.h"
#include "QXUtilsCommonUtil.h"

class QXServerWorker;

class QXServerMsgHandler{
private:
    std::atomic<uint32_t> TransId;
    QX_ERR_T RegisterClient(int32_t, QXServerWorker*, const QXSCMsg::MsgPayload);
public:
    QXServerMsgHandler();
    ~QXServerMsgHandler();
    void ProtoInitMsg(QXServerWorker*, QXSCMsg::MsgPayload&);
    void ProtoPreSend(QXSCMsg::MsgPayload&);
    void ProtoRelease(QXSCMsg::MsgPayload&);
    QX_ERR_T DispatchMsg(int32_t, QXServerWorker*, const QXSCMsg::MsgPayload);
};

#endif /* _QX_SERVER_MSG_BUSSINESS_H_ */
