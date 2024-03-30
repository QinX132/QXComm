#ifndef _QX_CLIENT_WORKER_H_
#define _QX_CLIENT_WORKER_H_
#include <iostream>
#include <map>
#include <vector>
#include <atomic>

#include "QXUtilsModuleCommon.h"

enum QXC_STATS {
    QXC_STATS_UNSPEC,
    QXC_STATS_INITED,
    QXC_STATS_CONNECTED,
    QXC_STATS_REGISTERED,
    QXC_STATS_DISCONNECTED,
    QXC_STATS_EXIT,
    QXC_STATS_MAX
};
    
#define QXC_IS_INITED(_stat_)                   ((_stat_) >= QXC_STATS_INITED && (_stat_) < QXC_STATS_EXIT)

typedef struct _QX_CLIENT_SERVER_CONF {
    std::string Addr;
    std::string Name;
    std::string Id;
}
QX_CLIENT_SERVER_CONF;

typedef struct _QX_CLIENT_WORKER_INIT_PARAM {
    uint32_t ClientId;
    std::vector<QX_CLIENT_SERVER_CONF> Servers;
}
QX_CLIENT_WORKER_INIT_PARAM;

class QXClientMsgHandler;

class QXClientWorker{
    friend class QXClientMsgHandler;
private:
    uint32_t ClientId;
    int ClientFd;
    int State;
    pthread_t MsgThreadId;
    pthread_t StateMachineThreadId;
    QX_CLIENT_SERVER_CONF CurrentServer;
    QX_CLIENT_WORKER_INIT_PARAM Param;
    struct event_base* EventBase;
    struct event* RecvEvent;
    QXClientMsgHandler *MsgHandler;

    QX_ERR_T InitClientFd();
    QX_ERR_T ConnectServer();
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
    QX_ERR_T Init(QX_CLIENT_WORKER_INIT_PARAM);
    void Exit();
};
#endif /* _QX_CLIENT_WORKER_H_ */