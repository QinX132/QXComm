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
}
QXC_WORKER_INIT_PARAM;

class QXClientMsgHandler;

class QXClientWorker{
    friend class QXClientMsgHandler;
private:
    int WorkerFd;
    QXC_WORKER_STATS State;
    pthread_t MsgThreadId;
    pthread_t StateMachineThreadId;
    QXC_WORKER_SERVER_CONF CurrentServer;
    QXC_WORKER_INIT_PARAM InitParam;
    struct event_base* EventBase;
    struct event* RecvEvent;
    struct event* KeepaliveEvent;
    QXClientMsgHandler *MsgHandler;

    QX_ERR_T InitWorkerFd();
    QX_ERR_T ConnectServer();
    void RecreateWorkerFd(void);
    QX_ERR_T RegisterToServer();
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void* _MsgThreadFn(void*);
    static void* _StateMachineThreadFn(void*);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    QX_ERR_T InitEventBaseAndRun();
    QX_ERR_T StartStateMachine();
public:
    QXClientWorker();
    ~QXClientWorker();
    QX_ERR_T Init(QXC_WORKER_INIT_PARAM);
    void Exit();
};
#endif /* _QX_CLIENT_WORKER_H_ */