#ifndef _QX_SERVER_WORKER_H_
#define _QX_SERVER_WORKER_H_
#include <iostream>
#include <map>

#include "QXSCMsg.pb.h"
#include "QXUtilsModuleCommon.h"
#include "QXServerMsgHandler.h"
#include "QXCommMngrClient.h"

typedef struct _QXS_WORKER_INIT_PARAM {
    uint16_t Port;
    std::string Name;
    uint32_t Load;
    std::string WorkPath;
    std::string Sm2PriKeyPwd;
}
QXS_WORKER_INIT_PARAM;

typedef struct _QXS_CLIENT_REGISTER_CTX {
    int Status;             // QXSCMsg::QX_SC_MSG_TYPE_REGISTER_*
    SM2_KEY PubKey;         // get this via QXCommMngr
    uint8_t ChallengeRandSend[16];
    int TransCipherSuite;   // QXSCMsg::CipherSuite
    union {
        uint8_t Sm4Key[QXUTIL_CRYPT_SM4_KEY_LEN];
    }TransKey;
}
QXS_CLIENT_REGISTER_CTX;

typedef struct _QXS_CLIENT_NODE {
    int Fd;
    uint32_t ClientId;
    int RecvMsgCnt;
    int ForwardMsgCnt;
    QXS_CLIENT_REGISTER_CTX RegisterCtx;
    struct epoll_event *RecvEvent;
}
QXS_CLIENT_NODE;

class QXServerWorker{
    friend class QXServerMsgHandler;
private:
    QXS_WORKER_INIT_PARAM InitParam;
    int WorkerFd;
    bool Inited;
    int MemId;
    int EpollFd;
    SM2_KEY Sm2PubKey;
    std::string Sm2PriKeyPath;
    struct epoll_event ListenEvent;
    pthread_t ThreadId;
    std::atomic<uint32_t> ClientCurrentNum;               // size of map is O(log n), so we use num
    std::map<int, uint32_t> ClientIdMap;
    pthread_spinlock_t Lock;   // this lock for client map

    QX_ERR_T InitPath(std::string);
    QX_ERR_T InitSm2Key(std::string, std::string);
    QX_ERR_T InitWorkerFd(uint16_t, uint32_t);
    QX_ERR_T InitClientMap();
    QX_ERR_T InitEventBaseAndRun();
    static void* _WorkerThreadFn(void*);
    void ServerAccept(void);
    void ClientRecv(int32_t);
    void EraseClient_NL(int32_t);
    void EraseClient(int32_t);
    QXServerMsgHandler *MsgHandler;
public:
    std::map<int, QXS_CLIENT_NODE*> ClientMap;
    QXCommMngrClient *MngrClient;
    void* Calloc(size_t);
    void Free(void*);
    QXServerWorker();
    ~QXServerWorker();
    QX_ERR_T Init(QXS_WORKER_INIT_PARAM);
    void Exit();
    std::string GetStatus();
};

#endif /* _QX_SERVER_WORKER_H_ */