#ifndef _QX_CLIENT_WORKER_H_
#define _QX_CLIENT_WORKER_H_
#include <iostream>
#include <map>
#include <vector>

#include "QXUtilsModuleCommon.h"

typedef struct _QX_CLIENT_SERVER_CONF {
    std::string Addr;
}
QX_CLIENT_SERVER_CONF;

typedef struct _QX_CLIENT_WORKER_INIT_PARAM {
    std::string WorkerName;
    std::vector<QX_CLIENT_SERVER_CONF> Servers;
}
QX_CLIENT_WORKER_INIT_PARAM;

class QXClientWorker{
private:
    uint32_t ClientId;
    int ClientFd;
    BOOL Inited;
    pthread_t ThreadId;
    QX_CLIENT_SERVER_CONF CurrentServer;
    QX_CLIENT_WORKER_INIT_PARAM Param;

    QX_ERR_T InitClientFd();
    QX_ERR_T ConnectServer();
public:
    QXClientWorker();
    ~QXClientWorker();
    QX_ERR_T Init(QX_CLIENT_WORKER_INIT_PARAM);
    void Exit();
};
#endif /* _QX_CLIENT_WORKER_H_ */