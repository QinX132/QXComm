#include <chrono>

#include "QXServerMsgHandler.h"
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

typedef struct _QXS_TPOOL_MSG_SEND_ARG{
    QXSCMsg::MsgPayload *SendMsg;
    int32_t Fd;
    QXServerWorker* Worker;
    QXServerMsgHandler* MsgHandler;
}
QXS_TPOOL_MSG_SEND_ARG;

extern "C" void QXServer_TPoolSendMsgFunc(void* Arg) {
    QXS_TPOOL_MSG_SEND_ARG *arg = (QXS_TPOOL_MSG_SEND_ARG*)Arg;
    QX_UTIL_Q_MSG *sendMsg = NULL;
    std::string serializedData;
    QX_ERR_T ret = QX_SUCCESS;
    uint8_t *cipher = NULL;
    size_t cipherLen = 0;
    const uint8_t* plain = NULL;
    size_t plainLen = 0;
    uint8_t iv[QXUTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);

    if (!arg || !arg->MsgHandler || !arg->SendMsg || !arg->Worker) {
        goto CommRet;
    }
    arg->MsgHandler->ProtoPreSend(*arg->SendMsg);
    serializedData = arg->SendMsg->SerializeAsString();

    if (arg->Worker->ClientMap[arg->Fd]->RegisterCtx.Status == QX_SC_MSG_TYPE_REGISTER_FINISH && 
        arg->Worker->ClientMap[arg->Fd]->RegisterCtx.TransCipherSuite == QX_SC_CIPHER_SUITE_SM4 &&
        arg->SendMsg->msgbase().msgtype() != QX_SC_MSG_TYPE_REGISTER_FINISH) {
        cipherLen = QXUtil_CryptSm4CBCGetPaddedLen(serializedData.size()) + sizeof(iv);
        cipher = (uint8_t*)arg->Worker->Calloc(cipherLen);
        if (!cipher) {
            goto CommRet;
        }
        plain = reinterpret_cast<const uint8_t*>(serializedData.c_str());
        plainLen = serializedData.size();
        QXUtil_Hexdump("Plain-Data", (uint8_t*)plain, plainLen);
        ret = QXUtil_CryptSm4CBCEncrypt(plain, plainLen, 
            arg->Worker->ClientMap[arg->Fd]->RegisterCtx.TransKey.Sm4Key, sizeof(arg->Worker->ClientMap[arg->Fd]->RegisterCtx.TransKey.Sm4Key), 
            cipher, &cipherLen, iv, &ivLen);
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
        QXUtil_Hexdump("SM4-Encrypt-Data", cipher, cipherLen);
    } else {
        sendMsg = QXUtil_NewSendQMsg(serializedData.size());
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    } 
    ret = QXUtil_SendQMsg(arg->Fd, sendMsg);
    if (ret < QX_SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", arg->SendMsg->ShortDebugString().c_str());

CommRet:
    if (sendMsg) 
        QXUtil_FreeSendQMsg(sendMsg);
    if (arg && arg->Worker && cipher)
        arg->Worker->Free(cipher);
    if (arg->SendMsg && arg->MsgHandler) {
        arg->MsgHandler->ProtoRelease(*arg->SendMsg);
        delete arg->SendMsg;
        arg->SendMsg = NULL;
    }
    if (arg && arg->Worker)
        arg->Worker->Free(arg);
    return ;
}

QX_ERR_T QXServerMsgHandler::SendMsgAsync(const MsgPayload MsgPayload, int32_t Fd) {
    QXS_TPOOL_MSG_SEND_ARG *tpoolArg = NULL;
    QX_ERR_T ret = QX_SUCCESS;
    
    tpoolArg = (QXS_TPOOL_MSG_SEND_ARG*)ServerWorker->Calloc(sizeof(QXS_TPOOL_MSG_SEND_ARG));
    if (!tpoolArg) {
        ret = -QX_ENOMEM;
        goto CommRet;
    }
    tpoolArg->SendMsg = new QXSCMsg::MsgPayload(MsgPayload);
    if (!tpoolArg->SendMsg) {
        goto CommRet;
    }
    tpoolArg->Fd = Fd;
    tpoolArg->MsgHandler = this;
    tpoolArg->Worker = ServerWorker;
    ret = QXUtil_TPoolAddTask(QXServer_TPoolSendMsgFunc, (void*)tpoolArg);
    if (ret < QX_SUCCESS) {
        goto CommRet;
    }

CommRet:
    if (ret) {
        ProtoRelease(*tpoolArg->SendMsg);
        if (tpoolArg->SendMsg)
            delete tpoolArg->SendMsg;
        tpoolArg->SendMsg = NULL;
        if (tpoolArg)
            ServerWorker->Free(tpoolArg);
    }
    return ret;
}

void QXServerMsgHandler::ProtoInitMsg(
    MsgPayload &MsgPayload
    )
{
    UNUSED(MsgPayload.mutable_msgbase());
    UNUSED(MsgPayload.mutable_serverinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_serverinfo()->set_servername(ServerWorker->InitParam.Name);
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
    UNUSED(MsgPayload.release_msgbase());
    UNUSED(MsgPayload.release_serverinfo());
}

QX_ERR_T QXServerMsgHandler::RegisterClient(
    int32_t Fd,
    const MsgPayload MsgPayload
    )
{
    QXSCMsg::MsgPayload sendMsgPayload;
    QX_ERR_T ret = QX_SUCCESS;
    uint8_t randTmpBuff[128];
    size_t randTmpBuffLen = sizeof(randTmpBuff);
    std::string receivedRand;
    uint8_t plainBuff[128];
    size_t plainBuffLen = sizeof(plainBuff);
    
    pthread_spin_lock(&ServerWorker->Lock);
    if (ServerWorker->ClientMap.count(Fd) <= 0) {
        ret = -QX_EEXIST;
        pthread_spin_unlock(&ServerWorker->Lock);
        goto CommRet;
    }
    pthread_spin_unlock(&ServerWorker->Lock);

    ProtoInitMsg(sendMsgPayload);

    if (MsgPayload.msgbase().msgtype() != (uint32_t)ServerWorker->ClientMap[Fd]->RegisterCtx.Status + 1 && 
        MsgPayload.msgbase().msgtype() != (uint32_t)ServerWorker->ClientMap[Fd]->RegisterCtx.Status - 1) {
        LogDbg("Ignore invalid type %u, current %d", MsgPayload.msgbase().msgtype(), ServerWorker->ClientMap[Fd]->RegisterCtx.Status);
        ret = -QX_EEXIST;
        goto CommRet;
    }

    switch (MsgPayload.msgbase().msgtype()) {
        case QX_SC_MSG_TYPE_REGISTER_REQUEST:
            ServerWorker->ClientMap[Fd]->ClientId = MsgPayload.msgbase().registerrequest().clientid();
            ServerWorker->ClientMap[Fd]->RegisterCtx.TransCipherSuite = MsgPayload.msgbase().registerrequest().ciphersuite();
            ret =ServerWorker->MngrClient->GetPubKeyAfterGotClientId(ServerWorker->ClientMap[Fd]->ClientId, ServerWorker->ClientMap[Fd]->RegisterCtx.PubKey);
            if (ret) {
                LogErr("Get pubKey failed! ret %d", ret);
                goto CommRet;
            }
            ret = QXUtil_CryptRandBytes(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend));
            if (ret) {
                LogErr("Gen Rand Bytes failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE Plain", ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend));
            ret = QXUtil_CryptSm2Encrypt(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend),
                    &ServerWorker->ClientMap[Fd]->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
            if (ret) {
                LogErr("Encrypt by pubkey failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_CHALLENGE Plain-Enc", randTmpBuff, randTmpBuffLen);
            sendMsgPayload.mutable_msgbase()->set_msgtype(QX_SC_MSG_TYPE_REGISTER_CHALLENGE);
            sendMsgPayload.mutable_msgbase()->mutable_registerchallenge()->set_cipherrand(randTmpBuff, randTmpBuffLen);
            ServerWorker->ClientMap[Fd]->RegisterCtx.Status = QX_SC_MSG_TYPE_REGISTER_CHALLENGE;
            break;
        case QX_SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
            // check send rand
            receivedRand = MsgPayload.msgbase().registerchallengereply().plainrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -QX_E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (randTmpBuffLen != sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend) || 
                memcmp(randTmpBuff, ServerWorker->ClientMap[Fd]->RegisterCtx.ChallengeRandSend, randTmpBuffLen) != 0) {
                ret = -QX_EIO;
                LogErr("Random mismatch!");
                goto CommRet;
            }
            QXUtil_Hexdump("CHALLENGE_REPLY Plain", randTmpBuff, randTmpBuffLen);
            // decrypt cipherrand and send
            randTmpBuffLen = sizeof(randTmpBuff);
            receivedRand = MsgPayload.msgbase().registerchallengereply().cipherrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -QX_E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_FINISH Cipher", randTmpBuff, randTmpBuffLen);
            plainBuffLen = sizeof(plainBuff);
            ret = QXUtil_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, ServerWorker->Sm2PriKeyPath.c_str(), ServerWorker->InitParam.Sm2PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
            if (ret) {
                LogErr("Decrypt by prikey failed! ret %d", ret);
                goto CommRet;
            }
            QXUtil_Hexdump("REGISTER_FINISH Cipher-Dec", plainBuff, plainBuffLen);
            sendMsgPayload.mutable_msgbase()->set_msgtype(QX_SC_MSG_TYPE_REGISTER_FINISH);
            sendMsgPayload.mutable_msgbase()->mutable_registerfinish()->set_plainrand(plainBuff, plainBuffLen);
            if (ServerWorker->ClientMap[Fd]->RegisterCtx.TransCipherSuite == QX_SC_CIPHER_SUITE_SM4) {
                ret = QXUtil_CryptRandBytes(ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key));
                if (ret) {
                    LogErr("Gen Rand Bytes failed! ret %d", ret);
                    goto CommRet;
                }
                randTmpBuffLen = sizeof(randTmpBuff);
                QXUtil_Hexdump("REGISTER_FINISH SM4-Key", ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key));
                ret = QXUtil_CryptSm2Encrypt(ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key, sizeof(ServerWorker->ClientMap[Fd]->RegisterCtx.TransKey.Sm4Key),
                        &ServerWorker->ClientMap[Fd]->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
                if (ret) {
                    LogErr("Encrypt failed! ret %d", ret);
                    goto CommRet;
                }
                QXUtil_Hexdump("REGISTER_FINISH SM4-Key-Enc", randTmpBuff, randTmpBuffLen);
                sendMsgPayload.mutable_msgbase()->
                    mutable_registerfinish()->mutable_ciphercontent()->set_ciphersm4key(randTmpBuff, randTmpBuffLen);
            } else {
                ret = -QX_ENOSYS;
                LogErr("Unsupported crypt suite %d", ServerWorker->ClientMap[Fd]->RegisterCtx.TransCipherSuite);
                goto CommRet;
            }
            sendMsgPayload.mutable_msgbase()->
                mutable_registerfinish()->mutable_ciphercontent()->set_ciphersuite(ServerWorker->ClientMap[Fd]->RegisterCtx.TransCipherSuite);
            ServerWorker->ClientMap[Fd]->RegisterCtx.Status = QX_SC_MSG_TYPE_REGISTER_FINISH;
            ServerWorker->ClientCurrentNum.fetch_add(1);
            if (ServerWorker->ClientFdMap.find(ServerWorker->ClientMap[Fd]->ClientId) == ServerWorker->ClientFdMap.end()) {
                ServerWorker->ClientFdMap[ServerWorker->ClientMap[Fd]->ClientId] = Fd;
                LogInfo("Insert id-fd:%u %d", ServerWorker->ClientMap[Fd]->ClientId, Fd);
            }
            break;
        default:
            LogErr("Ignore invalid type %u", MsgPayload.msgbase().msgtype());
            ret = -QX_EEXIST;
            goto CommRet;
    }
    ret = SendMsgAsync(sendMsgPayload, Fd);
    if (ret) {
        LogErr("Send msg async failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (ret) {
        ServerWorker->ClientMap[Fd]->RegisterCtx.Status = 0;
    }
    return ret;
}

QX_ERR_T QXServerMsgHandler::TransmitMsg(
    int32_t Fd,
    const MsgPayload RecvMsgPayload
    ) 
{
    int32_t peerFd = -1;    // todo
    QXSCMsg::MsgPayload sendMsgPayload;
    QX_ERR_T ret = QX_SUCCESS;
    
    if (ServerWorker->ClientFdMap.find(RecvMsgPayload.msgbase().transmsg().to().clientid()) == 
        ServerWorker->ClientFdMap.end()) {
        sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg("Peer not registered!");
        ret = -QX_EINVAL;
        LogErr("Peer not registered, clientid %d", RecvMsgPayload.msgbase().transmsg().to().clientid());
        goto CommErr;
    }

    peerFd = ServerWorker->ClientFdMap[RecvMsgPayload.msgbase().transmsg().to().clientid()];
    if (ServerWorker->ClientMap[Fd]->RegisterCtx.Status != QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH ||
        ServerWorker->ClientMap.find(peerFd) == ServerWorker->ClientMap.end() ||
        ServerWorker->ClientMap[peerFd]->RegisterCtx.Status != QXSCMsg::QX_SC_MSG_TYPE_REGISTER_FINISH) {
        sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg("Not registered, please wait or check!");
        ret = -QX_EINVAL;
        LogErr("Not registered, please wait or check!");
        goto CommErr;
    }

    sendMsgPayload.mutable_msgbase()->set_msgtype(QXSCMsg::QX_SC_MSG_TYPE_MSG_TRANS_S_2_C);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(
        RecvMsgPayload.msgbase().transmsg().to().clientid());
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(
        RecvMsgPayload.msgbase().transmsg().from().clientid());
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg(
        RecvMsgPayload.msgbase().transmsg().msg());
    goto CommRet;

CommErr:
    peerFd = Fd;
    sendMsgPayload.set_errcode(QX_ECONNREFUSED);
    sendMsgPayload.mutable_msgbase()->set_msgtype(QXSCMsg::QX_SC_MSG_TYPE_MSG_TRANS_S_2_C);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(
        RecvMsgPayload.msgbase().transmsg().from().clientid());
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(
        RecvMsgPayload.msgbase().transmsg().from().clientid());

CommRet:
    if (SendMsgAsync(sendMsgPayload, peerFd)) {
        LogErr("Send msg async failed!");
        goto CommRet;
    }
    return ret;
}

QX_ERR_T QXServerMsgHandler::DispatchMsg(
    int32_t Fd,
    const MsgPayload MsgPayload
    ) 
{
    QX_ERR_T ret = QX_SUCCESS;
    switch (MsgPayload.msgbase().msgtype()) {
        case QX_SC_MSG_TYPE_REGISTER_REQUEST:
        case QX_SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
            if (ServerWorker->ClientCurrentNum.load() < ServerWorker->InitParam.Load) {
                ret = RegisterClient(Fd, MsgPayload);
            } else {
                LogWarn("ClientNum has reached its upper limit %u, ignore connect request!", ServerWorker->ClientCurrentNum.load());
                ret = -QX_EBUSY;
            }
            break;
        case QX_SC_MSG_TYPE_MSG_TRANS_C_2_S:
            ret = TransmitMsg(Fd, MsgPayload);
            break;
        default:
            LogErr("Invalid type:%d", MsgPayload.msgbase().msgtype());
            return -QX_EIO;
    }
    return ret;
}

QXServerMsgHandler::QXServerMsgHandler(QXServerWorker* Worker):TransId(0){
    TransId.fetch_add(1);
    QXServerMsgHandler::ServerWorker = Worker;
};
QXServerMsgHandler::~QXServerMsgHandler(){};

