#ifndef _QX_SERVER_WORKER_H_
#define _QX_SERVER_WORKER_H_
#include <iostream>
#include <map>

#include "QXSCMsg.pb.h"
#include "QXUtilsModuleCommon.h"
#include "QXServerMsgBussiness.h"

typedef struct _QX_SERVER_WORKER_INIT_PARAM {
    std::pair<uint16_t, uint16_t> PortRange;
    std::string WorkerName;
    uint32_t WorkerLoad;
}
QX_SERVER_WORKER_INIT_PARAM;

typedef struct _QXS_CLIENT_NODE {
    int Fd;
    int ClientId;
    int RecvMsgCnt;
    int ForwardMsgCnt;
    bool Registered;
    struct epoll_event *RecvEvent;
    struct sockaddr_in ClientAddr;
}
QXS_CLIENT_NODE;

class QXServerWorker{
    friend class QXServerMsgHandler;
private:
    QX_SERVER_WORKER_INIT_PARAM InitParam;
    int ServerFd;
    bool Inited;
    int MemId;
    int EpollFd;
    struct epoll_event ListenEvent;
    pthread_t ThreadId;
    std::map<int, QXS_CLIENT_NODE*> ClientMap;
    std::map<int, uint32_t> ClientIdMap;
    pthread_spinlock_t Lock;   // this lock for client map

    QX_ERR_T InitServerFd(std::pair<uint16_t, uint16_t>, uint32_t);
    QX_ERR_T InitClientMap();
    QX_ERR_T InitEventBaseAndRun();
    static void* _WorkerThreadFn(void*);
    void ServerAccept(void);
    void ClientRecv(int32_t);
    void EraseClient_NL(int32_t);
    void EraseClient(int32_t);
    QXServerMsgHandler *MsgHandler;
public:
    void* Calloc(size_t);
    void Free(void*);
    QXServerWorker();
    ~QXServerWorker();
    QX_ERR_T Init(QX_SERVER_WORKER_INIT_PARAM);
    void Exit();
    std::string GetStatus();
};

#endif /* _QX_SERVER_WORKER_H_ */