#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "QXUtilsModuleCommon.h"
#include "QXSCShare.h"
#include "QXServerWorker.h"

using namespace std;
using json = nlohmann::json;

#define QX_SERVER_CONF_FILE_PATH                        "QXServerConf.json"
#define QX_SERVER_ROLE_NAME                             "QXServer"

static QXServerWorker *sg_ServerWorkers = NULL;
static QXCommMngrClient *sg_MngrClient = NULL;
static int sg_ServerWorkerTotalNum = 0;

int 
QXS_CmdGetAllClientMap(char* InBuff, size_t InBuffLen, char* OutBuff, size_t OutBuffLen) {
    size_t offset = 0;
    UNUSED(InBuff);
    UNUSED(InBuffLen);

    if (!sg_ServerWorkers || !sg_ServerWorkerTotalNum) {
        return QX_SUCCESS;
    }

    for (int loop = 0; loop < sg_ServerWorkerTotalNum; loop ++) {
        offset += snprintf(OutBuff + offset, OutBuffLen - offset, "%s", sg_ServerWorkers[loop].GetStatus().c_str());
    }

    return QX_SUCCESS;
}

extern "C" 
void 
QXS_MainExit(
    void
    )
{
    string cmd;
    if (sg_ServerWorkers) {
        for (sg_ServerWorkerTotalNum -= 1; sg_ServerWorkerTotalNum >= 0; sg_ServerWorkerTotalNum --) {
            sg_ServerWorkers[sg_ServerWorkerTotalNum].Exit();
        }
    }
    if (sg_MngrClient) {
        sg_MngrClient->Exit();
    }
    QXUtil_ModuleCommonExit();
    QX_ThirdPartyExit();
    cmd += string("killall ") + string(QX_SERVER_ROLE_NAME);
    system(cmd.c_str());
}

static QX_ERR_T _QXS_MainPreRegisterUtil(void) {
    QX_ERR_T ret = QX_SUCCESS;
    QX_UTIL_CMD_EXTERNAL_CONT cmdLineCont;
    // register out special cmd line
    memset(&cmdLineCont, 0 ,sizeof(cmdLineCont));
    cmdLineCont.Argc = 2;
    (void)snprintf(cmdLineCont.Opt, sizeof(cmdLineCont.Opt), "GetAllClientMap");
    (void)snprintf(cmdLineCont.Help, sizeof(cmdLineCont.Help), "Get server worker stats(ClientMap)");
    cmdLineCont.Cb = QXS_CmdGetAllClientMap;
    ret = QXUtil_CmdExternalRegister(cmdLineCont);
    if (ret < QX_SUCCESS) {
        LogErr("Register cmdline failed! ret %d", ret);
        return ret;
    }

    return ret;
}

static QX_ERR_T _QXS_MainInit(int Argc, char* Argv[]) {
    QX_ERR_T ret = QX_SUCCESS;
    string confFilePath(QX_SERVER_CONF_FILE_PATH);
    ifstream file(confFilePath);
    QX_UTIL_MODULES_INIT_PARAM initParam;
    string RoleName(QX_SERVER_ROLE_NAME);
    int32_t loop = 0;
    QXS_WORKER_INIT_PARAM workerInitParam;
    QX_COMM_MNGR_CLIENT_INIT_PARAM mngrClientConf;
    json fileJson;

    ret = QX_ThirdPartyInit();
    if (ret < QX_SUCCESS) {
        LogErr("Init third party conf failed! ret %d", ret);
        goto CommErr;
    }

    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -QX_ENOENT;
        goto CommErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -QX_EIO;
        LogErr("Parse json from %s failed!", confFilePath.c_str());
        goto CommErr;
    }
    // init param
    ret = QX_ParseConfFromJson(initParam, confFilePath, RoleName, Argc, Argv, QXS_MainExit);
    if (ret < QX_SUCCESS) {
        LogErr("Init param from conf failed! ret %d", ret);
        goto CommErr;
    }
    // pre register in utils
    ret = _QXS_MainPreRegisterUtil();
    if (ret < QX_SUCCESS) {
        LogErr("Register in util failed! ret %d", ret);
        goto CommErr;
    }
    // init utils modules
    ret = QXUtil_ModuleCommonInit(initParam);
    if (ret != QX_SUCCESS) {
        if (ret != -QX_ERR_EXIT_WITH_SUCCESS) {
            LogErr("Init utils modules failed! ret %d", ret);
            goto CommErr;
        }else {
            goto Success;
        }
    }
    // init mngr client
    try {
        sg_MngrClient = new QXCommMngrClient;
        mngrClientConf.ServerAddr = fileJson["MngrClientConf"]["ServerAddr"];
        if (fileJson["MngrClientConf"].contains("TrustCert") && 
            fileJson["MngrClientConf"]["TrustCert"].is_string() &&
            !fileJson["MngrClientConf"]["TrustCert"].get<std::string>().empty()) {
            mngrClientConf.TrustedCert = fileJson["MngrClientConf"]["TrustCert"];
        }
            
        ret = sg_MngrClient->Init(mngrClientConf);
        if (ret < QX_SUCCESS) {
            LogErr("Init mngr client failed! ret %d", ret);
            goto CommErr;
        }
    } catch (...){
        ret = -QX_EIO;
        LogErr("Init mngr client failed!");
        goto CommErr;
    }
    // init server workers
    try {
        loop = fileJson["ServerWorkerConf"]["WorkerNum"];
        sg_ServerWorkers = new QXServerWorker[loop];
        workerInitParam.PortRange.first = fileJson["ServerWorkerConf"]["WorkerPortStart"];
        workerInitParam.PortRange.second = workerInitParam.PortRange.first + loop - 1;
        workerInitParam.WorkerLoad = fileJson["ServerWorkerConf"]["LoadPerWorker"];
        for (loop -= 1 ; loop >= 0; loop --) {
            workerInitParam.WorkerName = "ServerWorker" + to_string(loop);
            ret = sg_ServerWorkers[loop].Init(workerInitParam);
            if (ret != QX_SUCCESS) {
                continue;
            }
            sg_ServerWorkerTotalNum ++;
        }
    } catch (...){
        ret = -QX_EIO;
        LogErr("Init server failed!");
        goto CommErr;
    }
    if (sg_ServerWorkerTotalNum <= 0){
        ret = -QX_ENOENT;
        goto CommErr;
    }

    goto Success;

CommErr:
    QXS_MainExit();
Success:
    if (file.is_open())
        file.close();
    return ret;
}
static void
_QXS_MainLoop(
    void
    )
{
    while(1) {
        sleep(1000);
    }
}
int main(
    int argc,
    char* argv[]
    )
{
    QX_ERR_T ret = QX_SUCCESS;

    ret = _QXS_MainInit(argc, argv);
    if (ret != QX_SUCCESS) {
        return ret;
    }

    _QXS_MainLoop();

    return ret;
}
