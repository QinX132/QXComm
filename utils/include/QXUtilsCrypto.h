#ifndef _QX_UTIL_CRYPTO_H_
#define _QX_UTIL_CRYPTO_H_

#ifdef __cplusplus
extern "C"{
#endif
#include "QXUtilsCommonUtil.h"

#include "gmssl/sm2.h"
#include "gmssl/sm3.h"
#include "gmssl/sm4.h"

#define QXUTIL_CRYPT_SM2_SIGN_LEN                           64  // sizeof(SM2_SIGNATURE)
#define QXUTIL_CRYPT_SM2_MAX_PLAIN_SIZE                     255 // SM2_MAX_PLAINTEXT_SIZE
#define QXUTIL_CRYPT_SM3_HASH_LEN                           32
#define QXUTIL_CRYPT_SM3_HMAC_LEN                           32  // SM3_HMAC_SIZE
#define QXUTIL_CRYPT_SM4_KEY_LEN                            16
#define QXUTIL_CRYPT_SM4_IV_LEN                             16
#define QXUTIL_CRYPT_SM4_BLOCK_SIZE                         16

QX_ERR_T
QXUtil_CryptRandBytes(
    __out uint8_t *RandBuf,
    __inout size_t RandLen
    );

QX_ERR_T
QXUtil_CryptSm2KeyGenAndExport(
    __in const char *PubKeyPath,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd
    );
    
QX_ERR_T
QXUtil_CryptSm2ImportPubKey(
    __in const char *PubKeyPath,
    __out SM2_KEY *PubKey
    );

QX_ERR_T
QXUtil_CryptSm2Sign(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd,
    __out uint8_t *Sign,
    __inout size_t *SignLen
    );

QX_ERR_T
QXUtil_CryptSm2Verify(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const SM2_KEY *PubKey,
    __in const uint8_t *Sign,
    __in size_t SignLen
    );

QX_ERR_T
QXUtil_CryptSm2Encrypt(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const SM2_KEY *PubKey,
    __out uint8_t *Cipher,
    __inout size_t *CipherLen
    );

QX_ERR_T
QXUtil_CryptSm2Decrypt(
    __in const uint8_t *Cipher,
    __in size_t CipherLen,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd,
    __out uint8_t *Plain,
    __inout size_t *PlainLen
    );

QX_ERR_T
QXUtil_CryptSm3Hash(
    __in const uint8_t *Input,
    __in size_t InputLen,
    __out uint8_t *Hash,
    __inout size_t *HashLen
    );

size_t
QXUtil_CryptSm4CBCGetPaddedLen(
    __in size_t PlainLen
    );

QX_ERR_T
QXUtil_CryptSm4CBCEncrypt(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const uint8_t *Key,
    __in size_t KeyLen,
    __out uint8_t *Cipher,
    __inout size_t *CipherLen,
    __out uint8_t *Iv,
    __inout size_t *IvLen
    );

QX_ERR_T
QXUtil_CryptSm4CBCDecrypt(
    __in const uint8_t *Cipher,
    __in size_t CipherLen,
    __in const uint8_t *Key,
    __in size_t KeyLen,
    __in uint8_t *Iv,
    __in size_t IvLen,
    __out uint8_t *Plain,
    __inout size_t *PlainLen
    );

#ifdef __cplusplus
}
#endif

#endif /* _QX_UTIL_CRYPTO_H_ */
