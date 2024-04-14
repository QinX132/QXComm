#ifndef _QX_COMM_MNGR_CLIENT_H_
#define _QX_COMM_MNGR_CLIENT_H_
#include <iostream>

typedef struct _QX_COMM_MNGR_CLIENT_INIT_PARAM {
    std::string MngrServerAddr;
    std::string MongoAddr;
    std::string RedisAddr;
}
QX_COMM_MNGR_CLIENT_INIT_PARAM;

class QXCommMngrClient {
private:
    QX_COMM_MNGR_CLIENT_INIT_PARAM InitParam;
    int32_t ClientFd;
    struct event_base *EventBase;
    struct event *KeepAliveEv;
    struct event *ListenEv;
public:
    QXCommMngrClient();
    ~QXCommMngrClient();
}

#endif /* _QX_COMM_MNGR_CLIENT_H_ */