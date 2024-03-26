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

int main()
{
    assert(QX_SUCCESS == _UT_CommonUtil_ByteOrderU64());

    return 0;
}
