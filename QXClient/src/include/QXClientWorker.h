#ifndef _QX_CLIENT_WORKER_H_
#define _QX_CLIENT_WORKER_H_
#include <iostream>
#include <map>
#include <vector>
#include <atomic>

#include "QXUtilsModuleCommon.h"

typedef enum _QXC_WORKER_STATS {
    QXC_WORKER_STATS_UNSPEC,
    QXC_WORKER_STATS_INITED,
    QXC_WORKER_STATS_CONNECTED,
    QXC_WORKER_STATS_REGISTERED,
    QXC_WORKER_STATS_DISCONNECTED,
    QXC_WORKER_STATS_EXIT,
    QXC_WORKER_STATS_MAX
}
QXC_WORKER_STATS;

#define QXC_IS_INITED(_stat_)                   ((_stat_) >= QXC_WORKER_STATS_INITED && (_stat_) < QXC_WORKER_STATS_EXIT)
#define QXC_SHOULD_EXIT(_stat_)                 ((_stat_) == QXC_WORKER_STATS_EXIT)

typedef struct _QXC_WORKER_SERVER_CONF {
    std::string Addr;
    std::string Name;
    std::string Id;
}
QXC_WORKER_SERVER_CONF;

typedef struct _QXC_WORKER_INIT_PARAM {
    uint32_t ClientId;
    std::vector<QXC_WORKER_SERVER_CONF> Servers;
    std::string WorkPath;
    std::string PriKeyPwd;
    uint32_t CryptoSuite;
}
QXC_WORKER_INIT_PARAM;

class QXClientMsgHandler;

typedef struct _QXS_CLIENT_REGISTER_CTX {
    int Status;             // QXSCMsg::QX_SC_MSG_TYPE_REGISTER_*
    SM2_KEY PubKey;         // get this via QXCommMngr
    uint8_t ChallengeRandSend[16];
    int TransCipherSuite;   // QXSCMsg::CipherSuite QX_SC_CIPHER_SUITE_SM4/QX_SC_CIPHER_SUITE_NONE
    union {
        uint8_t Sm4Key[QXUTIL_CRYPT_SM4_KEY_LEN];
    }TransKey;
    uint32_t RegisterRetried;
}
QXS_CLIENT_REGISTER_CTX;

class QXClientWorker{
    friend class QXClientMsgHandler;
private:
    int WorkerFd;
    QXC_WORKER_STATS State;
    pthread_t MsgThreadId;
    TIMER_HANDLE StatMachineTimer;
    QXC_WORKER_INIT_PARAM InitParam;
    struct event_base* EventBase;
    struct event* RecvEvent;
    struct event* KeepaliveEvent;
    QXClientMsgHandler *MsgHandler;
    QXS_CLIENT_REGISTER_CTX RegisterCtx;
    SM2_KEY PubKey;
    std::string PriKeyPath;
    int32_t CurrentServerPos;

    QX_ERR_T InitPath(std::string);
    QX_ERR_T InitSm2Key(std::string, std::string);
    QX_ERR_T InitWorkerFd();
    QX_ERR_T GetServerInfo();
    QX_ERR_T ConnectServer();
    void RecreateWorkerFd(void);
    QX_ERR_T RegisterToServer();
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void* _MsgThreadFn(void*);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    QX_ERR_T InitEventBaseAndRun();
    QX_ERR_T StartStateMachine();
    static void _StateMachineFn(evutil_socket_t Fd, short Ev, void* Arg);
public:
    QXClientWorker();
    ~QXClientWorker();
    QX_ERR_T SendMsg(uint32_t, std::string);
    QX_ERR_T Init(QXC_WORKER_INIT_PARAM);
    void Exit();
};
#endif /* _QX_CLIENT_WORKER_H_ */