#include "UnitTest.h"
#include "QXUtilsMem.h"

#define UT_MEM_REGISTER_NAME                        "UT_MEM"

static int 
_UT_Mem_ForwardT(
    void
    )
{
    int ret = QX_SUCCESS;
    int memId = QX_UTIL_MEM_MODULE_INVALID_ID;
    void* ptr = NULL;

    ret = QXUtil_MemModuleInit();
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

    ret = QXUtil_MemRegister(&memId, (char*)UT_MEM_REGISTER_NAME);
    if (ret)
    {
        UTLog("Register fail\n");
        goto CommonReturn;
    }

    ptr = QXUtil_MemCalloc(memId, sizeof(int));
    if (!ptr)
    {
        ret = -QX_ENOMEM;
        UTLog("calloc fail\n");
        goto CommonReturn;
    }
    
    if (ptr)
    {
        QXUtil_MemFree(memId, ptr);
    }
    if (!QXUtil_MemLeakSafetyCheckWithId(memId))
    {
        ret = -QX_EIO;
        UTLog("mem leak check fail\n");
        goto CommonReturn;
    }
    ret = QXUtil_MemUnRegister(&memId);
CommonReturn:
    if (QXUtil_MemModuleExit())
    {
        ret = -QX_EIO;
    }
    return ret;
}

int main()
{
    assert(QX_SUCCESS == _UT_Mem_ForwardT());

    return 0;
}

