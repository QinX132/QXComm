#include "UnitTest.h"
#include "QXUtilsCommonUtil.h"
#include "QXUtilsMem.h"
#include "QXUtilsLogIO.h"
#include "QXUtilsCrypto.h"

#define UT_CRYPT_PUBKEY_PATH                "PubKey.Pem"
#define UT_CRYPT_PRIKEY_PATH                "PriKey.Pem"
#define UT_CRYPT_PRIKEY_PWD                 "UnitTest"

static int
_UT_Crypt_PreInit(
    void
    )
{
    return QXUtil_MemModuleInit();
}

static int
_UT_Crypt_FinExit(
    void
    )
{
    return QXUtil_MemModuleExit();
}

static int 
_UT_Crypt_Sm2Test(
    void
    )
{
    QX_ERR_T ret = QX_SUCCESS;
    SM2_KEY sm2PubKey;
    uint8_t plain[] = 
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        ;
    size_t plainLen = sizeof(plain);
    uint8_t cipher[1024] = {0};
    size_t cipherLen = sizeof(cipher);
    uint8_t sign[QXUTIL_CRYPT_SM2_SIGN_LEN] = {0};
    size_t signLen = sizeof(sign);
    uint8_t decPlain[1024] = {0};
    size_t decPlainLen = sizeof(decPlain);

    QXUtil_Hexdump("plain", (uint8_t*)plain, plainLen);
    // gen sm2 key
    ret = QXUtil_CryptSm2KeyGenAndExport(UT_CRYPT_PUBKEY_PATH, UT_CRYPT_PRIKEY_PATH, UT_CRYPT_PRIKEY_PWD);
    if (ret) {
        UTLog("Key gen failed!\n");
        goto CommRet;
    }
    ret = QXUtil_CryptSm2ImportPubKey(UT_CRYPT_PUBKEY_PATH, &sm2PubKey);
    if (ret) {
        UTLog("PubKey get failed!\n");
        goto CommRet;
    }
    // sign / verify
    ret = QXUtil_CryptSm2Sign(plain, plainLen, UT_CRYPT_PRIKEY_PATH, UT_CRYPT_PRIKEY_PWD, sign, &signLen);
    if (ret) {
        UTLog("Sign failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("sign", (uint8_t*)sign, signLen);
    ret = QXUtil_CryptSm2Verify(plain, plainLen, &sm2PubKey, sign, signLen);
    if (ret) {
        UTLog("verify failed!\n");
        goto CommRet;
    }
    // enc / dec
    ret = QXUtil_CryptSm2Encrypt(plain, plainLen, &sm2PubKey, cipher, &cipherLen);
    if (ret) {
        UTLog("Encrypt failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("cipher", (uint8_t*)cipher, cipherLen);
    ret = QXUtil_CryptSm2Decrypt(cipher, cipherLen, UT_CRYPT_PRIKEY_PATH, UT_CRYPT_PRIKEY_PWD, decPlain, &decPlainLen);
    if (ret) {
        UTLog("Encrypt failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("decPlain", (uint8_t*)decPlain, decPlainLen);

    if (decPlainLen != plainLen || memcmp(decPlain, plain, plainLen) != 0) {
        ret = -QX_EIO;
        UTLog("decrypt mismatch!\n");
        goto CommRet;
    }

    UTLog("Sm2 test Success.\n");

CommRet:
    return ret;
}

static int 
_UT_Crypt_Sm3Test(
    void
    )
{
    QX_ERR_T ret = QX_SUCCESS;
    uint8_t plain[] = 
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        ;
    size_t plainLen = sizeof(plain);
    uint8_t actlHash[QXUTIL_CRYPT_SM3_HASH_LEN] = {0};
    size_t actlHashLen = sizeof(actlHash);
    uint8_t supposeHash[] = {
        0x2e, 0x90, 0xa3, 0xc8, 0x88, 0x30, 0x87, 0x0f, 0x87, 0x74, 0x04, 0xbf, 0xd9, 0xe0, 0xb7, 0x65, 
        0xbb, 0xb8, 0x98, 0x75, 0x96, 0xd5, 0xa3, 0x72, 0x55, 0xdd, 0x88, 0xc0, 0x3d, 0xd2, 0x6a, 0x20, 
    };

    QXUtil_Hexdump("plain", (uint8_t*)plain, plainLen);
    // sm3 hash test
    ret = QXUtil_CryptSm3Hash(plain, plainLen, actlHash, &actlHashLen);
    if (ret) {
        UTLog("hash test failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("hash", (uint8_t*)actlHash, actlHashLen);
    if (actlHashLen != QXUTIL_CRYPT_SM3_HASH_LEN || memcmp(actlHash, supposeHash, QXUTIL_CRYPT_SM3_HASH_LEN) != 0) {
        ret = -QX_EIO;
        UTLog("hash mismatch!\n");
        goto CommRet;
    }

    UTLog("Sm3 test Success.\n");

CommRet:
    return ret;
}

static int 
_UT_Crypt_Sm4Test(
    void
    )
{
    QX_ERR_T ret = QX_SUCCESS;
    uint8_t plain[] = 
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        "hello world 11111111233333333333333333333333333hello world 11111111233333333333333333333333333"
        ;
    size_t plainLen = sizeof(plain);
    uint8_t key[QXUTIL_CRYPT_SM4_KEY_LEN] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        0, 1, 2, 3, 4, 5, 6, 7
        };
    uint8_t keyLen = sizeof(key);
    uint8_t cipher[1024] = {0};
    size_t cipherLen = sizeof(cipher);
    uint8_t decPlain[1024] = {0};
    size_t decPlainLen = sizeof(decPlain);
    uint8_t iv[QXUTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);

    QXUtil_Hexdump("plain", (uint8_t*)plain, plainLen);
    // sm4 enc test
    ret = QXUtil_CryptSm4CBCEncrypt(plain, plainLen, key, keyLen, cipher, &cipherLen, iv, &ivLen);
    if (ret) {
        UTLog("sm4 encrypt test failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("cipher", (uint8_t*)cipher, cipherLen);
    QXUtil_Hexdump("iv", (uint8_t*)iv, ivLen);
    // sm4 dec test
    ret = QXUtil_CryptSm4CBCDecrypt(cipher, cipherLen, key, keyLen, iv, ivLen, decPlain, &decPlainLen);
    if (ret) {
        UTLog("sm4 encrypt test failed!\n");
        goto CommRet;
    }
    QXUtil_Hexdump("decPlain", (uint8_t*)decPlain, decPlainLen);
    
    if (decPlainLen != plainLen || memcmp(decPlain, plain, plainLen) != 0) {
        ret = -QX_EIO;
        UTLog("decrypt mismatch!\n");
        goto CommRet;
    }
    UTLog("Sm4 test Success.\n");

CommRet:
    return ret;
}

int main()
{
    assert(QX_SUCCESS == _UT_Crypt_PreInit());
    assert(QX_SUCCESS == _UT_Crypt_Sm2Test());
    assert(QX_SUCCESS == _UT_Crypt_Sm3Test());
    assert(QX_SUCCESS == _UT_Crypt_Sm4Test());
    assert(QX_SUCCESS == _UT_Crypt_FinExit());
    return 0;
}
