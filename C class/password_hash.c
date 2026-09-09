#define _CRT_SECURE_NO_WARNINGS

#include "password_hash.h"

/* clang-format off */
/* bcrypt.h 使用 windows.h 定义的 ULONG、PUCHAR 等类型，二者顺序不能对调。 */
#include <windows.h>
#include <bcrypt.h>
/* clang-format on */
#include <stdlib.h>
#include <string.h>

/* bcrypt.lib 是 Windows 自带的加密基础库，提供安全随机数和 SHA-256。 */
#pragma comment(lib, "bcrypt.lib")

#define SALT_BYTE_COUNT 16
#define SHA256_BYTE_COUNT 32

/* 一个字节拆成高 4 位和低 4 位，每 4 位转换为一个十六进制字符。 */
static void hex_encode(const unsigned char *input, int input_length, char *output)
{
    static const char HEX_DIGITS[] = "0123456789abcdef";
    int i;

    for (i = 0; i < input_length; ++i)
    {
        output[i * 2] = HEX_DIGITS[input[i] >> 4];
        output[i * 2 + 1] = HEX_DIGITS[input[i] & 0x0F];
    }
    output[input_length * 2] = '\0';
}

/* 把一个十六进制字符转换为 0~15；非法字符返回 -1。 */
static int hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/* 把两个十六进制字符还原为一个字节。 */
static int hex_decode(const char *input, unsigned char *output, int output_length)
{
    int i;

    if (!input || !output || strlen(input) != (size_t)output_length * 2)
        return 0;

    for (i = 0; i < output_length; ++i)
    {
        int high = hex_digit_value(input[i * 2]);
        int low = hex_digit_value(input[i * 2 + 1]);
        if (high < 0 || low < 0)
            return 0;
        output[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}

/*
 * 依次把“盐”和“明文密码”交给 Windows SHA-256 提供程序。
 * hash_object 是算法运行时需要的临时工作区；无论中途在哪一步失败，cleanup
 * 部分都会释放句柄和内存，避免资源泄漏。
 */
static int sha256_with_salt(const unsigned char *salt, int salt_length, const char *plain_password,
                            unsigned char output[SHA256_BYTE_COUNT])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char *hash_object = NULL;
    DWORD object_length = 0;
    DWORD hash_length = 0;
    DWORD bytes_written = 0;
    int success = 0;

    if (!salt || salt_length <= 0 || !plain_password || !output)
        return 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0)
        goto cleanup;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_length,
                          sizeof(object_length), &bytes_written, 0) != 0)
        goto cleanup;
    if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_length, sizeof(hash_length),
                          &bytes_written, 0) != 0 ||
        hash_length != SHA256_BYTE_COUNT)
        goto cleanup;

    hash_object = (unsigned char *)malloc(object_length);
    if (!hash_object)
        goto cleanup;
    if (BCryptCreateHash(algorithm, &hash, hash_object, object_length, NULL, 0, 0) != 0)
        goto cleanup;
    if (BCryptHashData(hash, (PUCHAR)salt, (ULONG)salt_length, 0) != 0)
        goto cleanup;
    if (BCryptHashData(hash, (PUCHAR)plain_password, (ULONG)strlen(plain_password), 0) != 0)
        goto cleanup;
    if (BCryptFinishHash(hash, output, SHA256_BYTE_COUNT, 0) != 0)
        goto cleanup;

    success = 1;

cleanup:
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    free(hash_object);
    return success;
}

/*
 * 不在遇到第一个不同字节时提前返回，而是完整比较 32 个字节。
 * 这样运行时间不会直接暴露“前多少个字节相同”。
 */
static int secure_equal(const unsigned char *left, const unsigned char *right, int length)
{
    unsigned char difference = 0;
    int i;

    for (i = 0; i < length; ++i)
        difference |= (unsigned char)(left[i] ^ right[i]);
    return difference == 0;
}

int password_hash_create(const char *plain_password, char salt_hex[PASSWORD_SALT_HEX_CAPACITY],
                         char hash_hex[PASSWORD_HASH_HEX_CAPACITY])
{
    unsigned char salt[SALT_BYTE_COUNT];
    unsigned char hash[SHA256_BYTE_COUNT];

    if (!plain_password || !plain_password[0] || !salt_hex || !hash_hex)
        return 0;
    if (BCryptGenRandom(NULL, salt, SALT_BYTE_COUNT, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return 0;
    if (!sha256_with_salt(salt, SALT_BYTE_COUNT, plain_password, hash))
        return 0;

    hex_encode(salt, SALT_BYTE_COUNT, salt_hex);
    hex_encode(hash, SHA256_BYTE_COUNT, hash_hex);
    return 1;
}

int password_hash_matches(const char *plain_password, const char *salt_hex, const char *hash_hex)
{
    unsigned char salt[SALT_BYTE_COUNT];
    unsigned char expected_hash[SHA256_BYTE_COUNT];
    unsigned char actual_hash[SHA256_BYTE_COUNT];

    if (!plain_password || !salt_hex || !hash_hex)
        return 0;
    if (!hex_decode(salt_hex, salt, SALT_BYTE_COUNT) ||
        !hex_decode(hash_hex, expected_hash, SHA256_BYTE_COUNT))
        return 0;
    if (!sha256_with_salt(salt, SALT_BYTE_COUNT, plain_password, actual_hash))
        return 0;
    return secure_equal(actual_hash, expected_hash, SHA256_BYTE_COUNT);
}
