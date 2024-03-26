#ifndef _QX_SERVER_WORKER_H_
#define _QX_SERVER_WORKER_H_
#include <iostream>
#include <map>

#include "QXUtilsModuleCommon.h"


typedef struct _QX_SERVER_WORKER_INIT_PARAM {
    std::pair<uint16_t, uint16_t> PortRange;
    std::string WorkerName;
    uint32_t WorkerLoad;
}
QX_SERVER_WORKER_INIT_PARAM;

typedef struct _QXS_CLIENT_NODE {
    int Fd;
    int ClientId;
    int ServerId;
    int RecvMsgCnt;
    int ForwardMsgCnt;
    BOOL Registered;
    struct event *RecvEvent;
    struct sockaddr_in ClientAddr;
}
QXS_CLIENT_NODE;

class QXServerWorker{
private:
    std::string ServerName;
    int ServerFd;
    BOOL Inited;
    int MemId;
    struct event_base* EventBase;
    struct event ListenEvent;
    pthread_t ThreadId;
    std::map<int, QXS_CLIENT_NODE*> ClientMap;
    pthread_spinlock_t Lock;   // this lock for client map

    QX_ERR_T InitServerFd(std::pair<uint16_t, uint16_t>, uint32_t);
    QX_ERR_T InitClientMap();
    QX_ERR_T InitEventBaseAndRun();
    static void* _WorkerThreadFn(void*);
    static void _ServerAccept(evutil_socket_t ,short ,void *);
    static void _ClientRecv(evutil_socket_t ,short ,void *);
public:
    QXServerWorker();
    ~QXServerWorker();
    void* Calloc(size_t);
    void Free(void*);
    QX_ERR_T Init(QX_SERVER_WORKER_INIT_PARAM);
    void Exit();
    std::string GetStatus();
};

#endif /* _QX_SERVER_WORKER_H_ */