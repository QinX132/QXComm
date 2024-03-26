#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "QXUtilsModuleCommon.h"
#include "QXServerWorker.h"

using namespace std;
using json = nlohmann::json;

#define QX_SERVER_CONF_FILE_PATH                        "QXServerConf.json"
#define QX_SERVER_RULE_NAME                             "QXServer"

static QXServerWorker *sg_ServerWorkers = NULL;
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
    QXUtil_ModuleCommonExit();
    cmd += string("killall ") + string(QX_SERVER_RULE_NAME);
    system(cmd.c_str());
}
static QX_ERR_T 
_QXS_MainInit(
    int Argc,
    char* Argv[]
    )
{
    QX_ERR_T ret = QX_SUCCESS;
    string ConfFilePath(QX_SERVER_CONF_FILE_PATH);
    ifstream file(ConfFilePath);
    json fileJson;
    QX_UTIL_MODULES_INIT_PARAM InitParam;
    string RuleName(QX_SERVER_RULE_NAME);
    int32_t loop = 0;
    QX_SERVER_WORKER_INIT_PARAM workerInitParam;
    QX_UTIL_CMD_EXTERNAL_CONT cmdLineCont;
    // load fileJson
    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -QX_ENOENT;
        goto FileOpenErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -QX_EIO;
        LogErr("Parse json from %s failed!", ConfFilePath.c_str());
        goto CommErr;
    }
    // init utils modules params
    try {
        InitParam.CmdLineArg = new QX_UTIL_CMDLINE_MODULE_INIT_ARG;
        memset(InitParam.CmdLineArg, 0, sizeof(QX_UTIL_CMDLINE_MODULE_INIT_ARG));
        if (fileJson.contains("LogConf") && !fileJson["LogConf"].is_null()) {
            InitParam.LogArg = new QX_UTIL_LOG_MODULE_INIT_ARG;
            memset(InitParam.LogArg, 0, sizeof(QX_UTIL_LOG_MODULE_INIT_ARG));
        }
        if (fileJson.contains("HealthCheckConf") && !fileJson["HealthCheckConf"].is_null()) {
            InitParam.HealthArg = new QX_UTIL_HEALTH_MODULE_INIT_ARG;
            memset(InitParam.HealthArg, 0, sizeof(QX_UTIL_HEALTH_MODULE_INIT_ARG));
        }
        if (fileJson.contains("TPoolConf") && !fileJson["TPoolConf"].is_null()) {
            InitParam.TPoolArg = new QX_UTIL_TPOOL_MODULE_INIT_ARG;
            memset(InitParam.TPoolArg, 0, sizeof(QX_UTIL_TPOOL_MODULE_INIT_ARG));
        }
    } catch(const std::exception& e) {
        ret = -QX_EIO;
        LogErr("New conf failed!");
        goto CommErr;
    }
    InitParam.CmdLineArg->Argc = Argc;
    InitParam.CmdLineArg->Argv = Argv;
    InitParam.CmdLineArg->ExitFunc = QXS_MainExit;
    copy(RuleName.begin(), RuleName.end(), InitParam.CmdLineArg->RoleName);
    if (InitParam.LogArg) {
        try {
            string tmpPath = fileJson["LogConf"]["LogPath"];
            copy(tmpPath.begin(), tmpPath.end(), InitParam.LogArg->LogFilePath);
            InitParam.LogArg->LogLevel = fileJson["LogConf"]["LogLevel"];
            InitParam.LogArg->LogMaxSize = fileJson["LogConf"]["LogSize"];
            InitParam.LogArg->LogMaxNum = fileJson["LogConf"]["LogMaxNum"];
            copy(RuleName.begin(), RuleName.end(), InitParam.LogArg->RoleName);
        } catch (const std::exception& e){
            ret = -QX_EIO;
            LogErr("Init Log conf failed!");
            goto CommErr;
        }
    }
    if (InitParam.TPoolArg) {
        try {
            InitParam.TPoolArg->TaskListMaxLength = fileJson["TPoolConf"]["TPoolQueueMaxLen"];
            InitParam.TPoolArg->ThreadPoolSize = fileJson["TPoolConf"]["TPoolSize"];
            InitParam.TPoolArg->Timeout = fileJson["TPoolConf"]["TPoolTimeout"];
        } catch (const std::exception& e){
            ret = -QX_EIO;
            LogErr("Init TPool conf failed!");
            goto CommErr;
        }
    }
    if (InitParam.HealthArg) {
        try {
            InitParam.HealthArg->CmdLineHealthIntervalS = fileJson["HealthCheckConf"]["CmdLineHealthIntervalS"];
            InitParam.HealthArg->LogHealthIntervalS = fileJson["HealthCheckConf"]["LogHealthIntervalS"];
            InitParam.HealthArg->MemHealthIntervalS = fileJson["HealthCheckConf"]["MemHealthIntervalS"];
            InitParam.HealthArg->MHHealthIntervalS = fileJson["HealthCheckConf"]["MHHealthIntervalS"];
            InitParam.HealthArg->MsgHealthIntervalS = fileJson["HealthCheckConf"]["MsgHealthIntervalS"];
            InitParam.HealthArg->TimerHealthIntervalS = fileJson["HealthCheckConf"]["TimerHealthIntervalS"];
            InitParam.HealthArg->TPoolHealthIntervalS = fileJson["HealthCheckConf"]["TPoolHealthIntervalS"];
        } catch (const std::exception& e){
            ret = -QX_EIO;
            LogErr("Init Health conf failed!");
            goto CommErr;
        }
    }
    InitParam.InitMsgModule = TRUE;
    InitParam.InitTimerModule = TRUE;
    // register out special cmd line
    memset(&cmdLineCont, 0 ,sizeof(cmdLineCont));
    cmdLineCont.Argc = 2;
    (void)snprintf(cmdLineCont.Opt, sizeof(cmdLineCont.Opt), "GetAllClientMap");
    (void)snprintf(cmdLineCont.Help, sizeof(cmdLineCont.Help), "Get server worker stats(ClientMap)");
    cmdLineCont.Cb = QXS_CmdGetAllClientMap;
    ret = QXUtil_CmdExternalRegister(cmdLineCont);
    if (ret != QX_SUCCESS) {
        LogErr("Register cmdline failed! ret %d", ret);
        goto CommErr;
    }
    // init utils modules
    ret = QXUtil_ModuleCommonInit(InitParam);
    if (ret != QX_SUCCESS) {
        if (ret != -QX_ERR_EXIT_WITH_SUCCESS)
            LogErr("Init utils modules failed! ret %d", ret);
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
    }

CommErr:
    file.close();
FileOpenErr:
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
