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
#define QX_CLIENT_TRUSTED_CONF_FILE_PATH                "QXClientTrust.json"
#define QX_CLIENT_RULE_NAME                             "QXClient"

static QXClientWorker sg_ClientWorker;

static void _QXC_MainExit(void) {
    sg_ClientWorker.Exit();
    QXUtil_ModuleCommonExit();
}

static QX_ERR_T _QXC_MainGetSpecailConfFromJson(
    json FileJson,
    QXC_WORKER_INIT_PARAM &InitParam
    ) 
{
    // init client workers
    if (FileJson.find("ServerConf") != FileJson.end()) {
        json serverArray = FileJson["ServerConf"];
        for (const auto& server : serverArray) {
            QXC_WORKER_SERVER_CONF serverConf;
            serverConf.Addr = server["Addr"];
            InitParam.Servers.push_back(serverConf);
        }
    } else {
        LogErr("Array 'ServerConf' not found in JSON");
        return -QX_EIO;
    }
    if (!FileJson.contains("ClientConf") || FileJson["ClientConf"].is_null()) {
        LogErr("Parse ClientConf from conf failed!");
        return -QX_EIO;
    }
    if (!FileJson["ClientConf"].contains("Id") || FileJson["ClientConf"]["Id"].is_null()) {
        LogErr("Parse Id from ClientConf failed!");
        return -QX_EIO;
    }
    InitParam.ClientId = FileJson["ClientConf"]["Id"];
    return QX_SUCCESS;
}

static QX_ERR_T _QXC_MainInit(int Argc, char* Argv[]) {
    QX_ERR_T ret = QX_SUCCESS;
    string roleName(QX_CLIENT_RULE_NAME);
    string confPath(QX_CLIENT_CONF_FILE_PATH);
    ifstream file(confPath);
    QX_UTIL_MODULES_INIT_PARAM initParam;
    json fileJson;
    QXC_WORKER_INIT_PARAM clientWorkerParam;
    
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
    // get client init param
    ret = _QXC_MainGetSpecailConfFromJson(fileJson, clientWorkerParam);
    if (ret < QX_SUCCESS) {
        LogErr("Get client param from conf failed! ret %d", ret);
        goto CommRet;
    }
    // init client
    ret = sg_ClientWorker.Init(clientWorkerParam);
    if (ret < QX_SUCCESS) {
        LogErr("Init client failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    file.close();
FileOpenErr:
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
