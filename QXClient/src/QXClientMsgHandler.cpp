#include <chrono>

#include "QXClientMsgHandler.h"
#include "QXClientWorker.h"

using namespace QXSCMsg;

void QXClientMsgHandler::ProtoInitMsg(
    MsgPayload &MsgPayload
    )
{
    UNUSED(MsgPayload.mutable_msgbase());
    UNUSED(MsgPayload.mutable_clientinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_clientinfo()->set_clientid(ClientWorker->InitParam.ClientId);
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
    UNUSED(MsgPayload.release_msgbase());
    UNUSED(MsgPayload.release_clientinfo());
}

QX_ERR_T QXClientMsgHandler::CreateRegisterProtoMsg(
    MsgBase &MsgBase
    ) 
{
    QX_ERR_T ret = QX_SUCCESS;

    switch (ClientWorker->RegisterCtx.Status) {
        case 0:
        case QX_SC_MSG_TYPE_REGISTER_REQUEST:
            MsgBase.set_msgtype(QX_SC_MSG_TYPE_REGISTER_REQUEST);
            MsgBase.mutable_registerrequest()->set_clientid(ClientWorker->InitParam.ClientId);
            ClientWorker->RegisterCtx.TransCipherSuite = ClientWorker->InitParam.CryptoSuite;
            MsgBase.mutable_registerrequest()->set_ciphersuite(ClientWorker->RegisterCtx.TransCipherSuite);
            ClientWorker->RegisterCtx.Status = QX_SC_MSG_TYPE_REGISTER_REQUEST;
            break;
        case QX_SC_MSG_TYPE_REGISTER_CHALLENGE:
        case QX_SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
        case QX_SC_MSG_TYPE_REGISTER_FINISH:
            ret = -QX_EINVAL;
            break;
    }

    return ret;
}

QX_ERR_T QXClientMsgHandler::DispatchMsg(MsgPayload MsgPayload) {
    QX_ERR_T ret = QX_SUCCESS;
    uint8_t randTmpBuff[128];
    size_t randTmpBuffLen = sizeof(randTmpBuff);
    std::string receivedRand;
    std::string serializedData;
    uint8_t plainBuff[128];
    size_t plainBuffLen = sizeof(plainBuff);
    QXSCMsg::MsgPayload sendMsg;
    QX_UTIL_Q_MSG *sendQMsg = NULL;
    
    ProtoInitMsg(sendMsg);


    switch (MsgPayload.msgbase().msgtype()) {
        case QX_SC_MSG_TYPE_REGISTER_CHALLENGE:
            if (MsgPayload.errcode() != 0) {
                ClientWorker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // decrypt cipherrand and send
            receivedRand = MsgPayload.msgbase().registerchallenge().cipherrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -QX_E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE CipherRand", randTmpBuff, randTmpBuffLen);
            ret = QXUtil_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, ClientWorker->PriKeyPath.c_str(), ClientWorker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
            if (ret) {
                LogErr("Decrypt by prikey failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE CipherRand-Dec", plainBuff, plainBuffLen);
            ret = QXUtil_CryptRandBytes(ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend));
            if (ret) {
                LogErr("Gen Rand Bytes failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE PlainRand", ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend));
            randTmpBuffLen = sizeof(randTmpBuff);
            ret = QXUtil_CryptSm2Encrypt(ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend),
                    &ClientWorker->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
            if (ret) {
                LogErr("Encrypt by pubkey failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE PlainRand-Enc", randTmpBuff, randTmpBuffLen);
            sendMsg.mutable_msgbase()->set_msgtype(QX_SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY);
            sendMsg.mutable_msgbase()->mutable_registerchallengereply()->set_plainrand(plainBuff, plainBuffLen);
            sendMsg.mutable_msgbase()->mutable_registerchallengereply()->set_cipherrand(randTmpBuff, randTmpBuffLen);
            ProtoPreSend(sendMsg);
            serializedData = sendMsg.SerializeAsString();
            sendQMsg = QXUtil_NewSendQMsg(serializedData.size());
            if (!sendQMsg) {
                goto CommRet;
            }
            memcpy(sendQMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
            ret = QXUtil_SendQMsg(ClientWorker->WorkerFd, sendQMsg);
            if (ret < QX_SUCCESS) {
                LogErr("send msg failed! ret %d", ret);
                goto CommRet;
            }
            LogInfo("Send Msg: %s", sendMsg.ShortDebugString().c_str());
            break;
        case QX_SC_MSG_TYPE_REGISTER_FINISH:
            if (MsgPayload.errcode() != 0) {
                ClientWorker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // check decrypt rand
            receivedRand = MsgPayload.msgbase().registerfinish().plainrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -QX_E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (randTmpBuffLen != sizeof(ClientWorker->RegisterCtx.ChallengeRandSend) || 
                memcmp(randTmpBuff, ClientWorker->RegisterCtx.ChallengeRandSend, randTmpBuffLen) != 0) {
                ret = -QX_EIO;
                LogErr("Random mismatch!");
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_FINISH PlainRand", randTmpBuff, randTmpBuffLen);
            // decrypt cipherrand and send
            randTmpBuffLen = sizeof(randTmpBuff);
            receivedRand = MsgPayload.msgbase().registerfinish().ciphercontent().ciphersm4key();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -QX_E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (ClientWorker->RegisterCtx.TransCipherSuite != (int)MsgPayload.msgbase().registerfinish().ciphercontent().ciphersuite()) {
                ret = -QX_EIO;
                LogErr("Crypto suite mismatch!\n");
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_FINISH SM4-Key-Enc", randTmpBuff, randTmpBuffLen);
            if (ClientWorker->RegisterCtx.TransCipherSuite == QX_SC_CIPHER_SUITE_SM4) {
                ret = QXUtil_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, ClientWorker->PriKeyPath.c_str(), ClientWorker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
                if (ret || plainBuffLen != sizeof(ClientWorker->RegisterCtx.TransKey.Sm4Key)) {
                    ret = -QX_EIO;
                    LogErr("decrypt failed!\n");
                    goto CommRet;
                }
                memcpy(ClientWorker->RegisterCtx.TransKey.Sm4Key, plainBuff, plainBuffLen);
                QXUtil_Hexdump("REGISTER_FINISH SM4-Key", plainBuff, plainBuffLen);
            }
            ClientWorker->RegisterCtx.Status = QX_SC_MSG_TYPE_REGISTER_FINISH;
            ClientWorker->State = QXC_WORKER_STATS_REGISTERED;
            LogInfo("Register to server success, set status as registered.");
            break;
        case QX_SC_MSG_TYPE_MSG_TRANS_S_2_C:
            std::cout << "Recv from client-" << MsgPayload.msgbase().transmsg().from().clientid() << ":" << MsgPayload.msgbase().transmsg().msg() << '\n';
            break;
        default:
            ret = -QX_EINVAL;
            goto CommRet;
    }

CommRet:
    ProtoRelease(sendMsg);
    if (sendQMsg) 
        QXUtil_FreeSendQMsg(sendQMsg);
    return ret;
}

QXClientMsgHandler::QXClientMsgHandler(QXClientWorker *Worker):TransId(0){
    TransId.fetch_add(1);
    QXClientMsgHandler::ClientWorker = Worker;
};
QXClientMsgHandler::~QXClientMsgHandler(){};
