#ifndef _QX_SERVER_MSG_HANDLER_H_
#define _QX_SERVER_MSG_HANDLER_H_

#include <atomic>
#include "QXSCMsg.pb.h"
#include "QXUtilsCommonUtil.h"

class QXServerWorker;

class QXServerMsgHandler{
private:
    std::atomic<uint32_t> TransId;
    QX_ERR_T RegisterClient(int32_t, const QXSCMsg::MsgPayload);
    QX_ERR_T SendMsgAsync(const QXSCMsg::MsgPayload, int32_t);
    QX_ERR_T TransmitMsg(int32_t, const QXSCMsg::MsgPayload);
    QXServerWorker* ServerWorker;
public:
    QXServerMsgHandler(QXServerWorker*);
    ~QXServerMsgHandler();
    void ProtoInitMsg(QXSCMsg::MsgPayload&);
    void ProtoPreSend(QXSCMsg::MsgPayload&);
    void ProtoRelease(QXSCMsg::MsgPayload&);
    QX_ERR_T DispatchMsg(int32_t, const QXSCMsg::MsgPayload);
};

#endif /* _QX_SERVER_MSG_HANDLER_H_ */
