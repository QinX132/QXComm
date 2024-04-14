#ifndef _QX_SC_SHARE_H_
#define _QX_SC_SHARE_H_

#include <openssl/err.h>
#include <openssl/ssl.h>
#include "QXUtilsModuleCommon.h"
QX_ERR_T QX_ParseConfFromJson(QX_UTIL_MODULES_INIT_PARAM &InitParam, std::string ConfFilePath, std::string RoleName, int Argc, char **Argv, ExitHandle ExitHandle) ;
QX_ERR_T QX_ThirdPartyInit(void);
void QX_ThirdPartyExit(void);
QX_ERR_T QX_SSLErrorShow(SSL* SSL, int Res);
#endif /* _QX_SC_SHARE_H_ */