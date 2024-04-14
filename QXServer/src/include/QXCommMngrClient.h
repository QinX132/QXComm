#ifndef _QX_COMM_MNGR_CLIENT_H_
#define _QX_COMM_MNGR_CLIENT_H_
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "QXUtilsModuleCommon.h"

typedef struct _QX_COMM_MNGR_CLIENT_INIT_PARAM {
    std::string ServerAddr;
    std::string TrustedCert;
}
QX_COMM_MNGR_CLIENT_INIT_PARAM;

typedef enum _QXMNGR_CLIENT_STATS {
    QXMNGR_CLIENT_STATS_UNSPEC,
    QXMNGR_CLIENT_STATS_INITED,
    QXMNGR_CLIENT_STATS_CONNECTED,
    QXMNGR_CLIENT_STATS_REGISTERED,
    QXMNGR_CLIENT_STATS_DISCONNECTED,
    QXMNGR_CLIENT_STATS_EXIT,
    QXMNGR_CLIENT_STATS_MAX
}
QXMNGR_CLIENT_STATS;

#define QXMNGR_C_IS_INITED(_stat_)                   ((_stat_) >= QXMNGR_CLIENT_STATS_INITED && (_stat_) < QXMNGR_CLIENT_STATS_EXIT)
#define QXMNGR_C_SHOULD_EXIT(_stat_)                 ((_stat_) == QXMNGR_CLIENT_STATS_EXIT)

class QXCommMngrClient{
private:
    QX_COMM_MNGR_CLIENT_INIT_PARAM InitParam;
    int32_t ClientFd;
    SSL* ClientSSL;
    SSL_CTX* ClientCtx;
    pthread_t ThreadId;
    struct event_base *EventBase;
    struct event *KeepaliveEvent;
    struct event *HealthMonitorEvent;
    struct event *RecvEvent;
    QXMNGR_CLIENT_STATS State; 

    static void* _MngrCThreadFn(void*);
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void _Keepalive(evutil_socket_t ,short ,void *);
    static void _HealthMonitor(evutil_socket_t ,short ,void *);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    QX_ERR_T InitClientFd();
    void CloseClientFd();
    QX_ERR_T ConnectServer();
    QX_ERR_T InitEventBaseAndRun();

public:
    QX_ERR_T Init(QX_COMM_MNGR_CLIENT_INIT_PARAM);
    void Exit();
    QXCommMngrClient();
    ~QXCommMngrClient();
};

#endif /* _QX_COMM_MNGR_CLIENT_H_ */