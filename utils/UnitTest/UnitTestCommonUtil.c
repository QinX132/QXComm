#include "UnitTest.h"
#include "QXUtilsCommonUtil.h"

static int
_UT_CommonUtil_ByteOrderU64(
    void
    )
{
    int ret = QX_SUCCESS;
    uint64_t before = 0x12345678, afterShouldBe = 0x78563412, after = 0;

    after = QXUtil_htonll(before);
    if (after != afterShouldBe || QXUtil_ntohll(after) != before)
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}

static int
_UT_CommonUtil_ParseIp(
    void
    )
{
    int ret = QX_SUCCESS;
    char* ipStr = "192.168.137.101";
    char* ipStrWithPort = "192.168.137.101:443";
    uint32_t ip = 0;
    uint16_t port = 0;

    ret = QXUtil_ParseStringToIpv4(ipStr, strlen(ipStr), &ip);
    if (ret < QX_SUCCESS || ip != 3232270693)
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    ret = QXUtil_ParseStringToIpv4AndPort(ipStrWithPort, strlen(ipStrWithPort), &ip, &port);
    if (ret < QX_SUCCESS || ip != 3232270693 || port != 443)
    {
        ret = -QX_EIO;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}
int main()
{
    assert(QX_SUCCESS == _UT_CommonUtil_ByteOrderU64());

    return 0;
}
