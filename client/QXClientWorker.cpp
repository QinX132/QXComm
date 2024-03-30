#include "QXClientWorker.h"
#include "QXUtilsCommonUtil.h"

using namespace std;

QX_ERR_T QXClientWorker::ConnectServer(void) {
    QX_ERR_T ret = QX_SUCCESS;
    BOOL connectSuccess = FALSE;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    for(auto serverConf:Param.Servers) {
        ret = QXUtil_ParseStringToIpv4AndPort(serverConf.Addr.c_str(), serverConf.Addr.length(), &ip, &port);
        if (ret < QX_SUCCESS || ip == 0 || port == 0) {
            LogErr("Parse %s failed!", serverConf.Addr.c_str());
            continue;
        }
        serverAddr.sin_addr.s_addr = htonl(ip);
        serverAddr.sin_port = htons(port);
        if (connect(ClientFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            LogErr("Connect to %s failed, try next one", serverConf.Addr.c_str());
            continue;
        } else {
            connectSuccess = TRUE;
            CurrentServer = serverConf;
            break;
        }
    }

    if (!connectSuccess) {
        LogErr("Connect faield!");
        ret = -QX_ENOTCONN;
        goto CommRet;
    }

CommRet:
    return ret;
}

QX_ERR_T QXClientWorker::InitClientFd(void) {
    QX_ERR_T ret = QX_SUCCESS;
    struct timeval tv;
    int32_t reuseable = 1, flags = 0; // set port reuseable when fd closed
    // socket
    ClientFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientFd < 0) {
        LogErr("Socket failed!");
        ret = -QX_EBADFD; 
        goto CommRet;
    }
    // reuse && nonblock
    if (setsockopt(ClientFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags = fcntl(ClientFd, F_GETFL, 0);
    if (flags == -1) {
        LogErr("get flags failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    flags |= O_NONBLOCK;
    if (fcntl(ClientFd, F_SETFL, flags) == -1) {
        LogErr("set nonblock failed!");
        ret = -QX_EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommErr;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommErr;
    }
    goto CommRet;
    
CommErr:
    close(ClientFd);
    ClientFd = -1;
CommRet:
    return ret;
} 

QX_ERR_T QXClientWorker::Init(QX_CLIENT_WORKER_INIT_PARAM InitParam){
    QX_ERR_T ret = QX_SUCCESS;

    Param = InitParam;
    ret = InitClientFd();
    if (ret < QX_SUCCESS) {
        LogErr("Init fd failed! ret %d", ret);
        goto InitErr;
    }
    ret = ConnectServer();
    if (ret < QX_SUCCESS) {
        LogErr("Connect failed! ret %d", ret);
        goto InitErr;
    }

    goto CommRet;

InitErr:
    Exit();
CommRet:
    return ret;
}

void QXClientWorker::Exit(void) {
    if (Inited) {
        if (ClientFd) {
            close(ClientFd);
            ClientFd = -1;
        }
    }
}

QXClientWorker::QXClientWorker() {
    ClientFd = -1;
    Inited = FALSE;
}

QXClientWorker::~QXClientWorker() {
    if (ClientFd != -1) {
        close(ClientFd);
    }
}