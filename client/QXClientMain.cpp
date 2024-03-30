#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "QXUtilsModuleCommon.h"
#include "QXSCShare.h"
#include "QXClientWorker.h"

using namespace std;
using json = nlohmann::json;

#define QX_CLIENT_CONF_FILE_PATH                        "QXClientConf.json"
#define QX_CLIENT_RULE_NAME                             "QXClient"

static QXClientWorker sg_ClientWorker;

static void _QXC_MainExit(void) {
    sg_ClientWorker.Exit();
    QXUtil_ModuleCommonExit();
}

static QX_ERR_T _QXC_MainInit(int Argc, char* Argv[]) {
    QX_ERR_T ret = QX_SUCCESS;
    string roleName(QX_CLIENT_RULE_NAME);
    string confPath(QX_CLIENT_CONF_FILE_PATH);
    ifstream file(confPath);
    QX_UTIL_MODULES_INIT_PARAM initParam;
    json fileJson;
    QX_CLIENT_WORKER_INIT_PARAM clientWorkerParam;
    
    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -QX_ENOENT;
        goto FileOpenErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -QX_EIO;
        LogErr("Parse json from %s failed!", confPath.c_str());
        goto CommRet;
    }
    // init param
    ret = QX_ParseConfFromJson(initParam, confPath, roleName, Argc, Argv, _QXC_MainExit);
    if (ret < QX_SUCCESS) {
        LogErr("Init param from conf failed! ret %d", ret);
        goto CommRet;
    }
    // init utils modules
    ret = QXUtil_ModuleCommonInit(initParam);
    if (ret < QX_SUCCESS) {
        if (ret != -QX_ERR_EXIT_WITH_SUCCESS)
            LogErr("Init utils modules failed! ret %d", ret);
        goto CommRet;
    }
    // init client workers
    clientWorkerParam.WorkerName = "ClientWorker0";
    if (fileJson.find("Server") != fileJson.end()) {
        json serverArray = fileJson["Server"];
        for (const auto& server : serverArray) {
            QX_CLIENT_SERVER_CONF serverConf;
            serverConf.Addr = server["Addr"];
            clientWorkerParam.Servers.push_back(serverConf);
        }
        std::cout << std::endl;
    } else {
        LogErr("Array 'Server' not found in JSON", ret);
    }
    sg_ClientWorker.Init(clientWorkerParam);

FileOpenErr:
    file.close();
CommRet:
    return ret;
}
static void
_QXC_MainLoop(
    void
    )
{
    while(1) {
        sleep(1000);
    }
}
int main(int argc, char* argv[]) {
    QX_ERR_T ret = QX_SUCCESS;

    ret = _QXC_MainInit(argc, argv);
    if (ret != QX_SUCCESS) {
        return ret;
    }

    _QXC_MainLoop();

    _QXC_MainExit();
    return ret;
}
