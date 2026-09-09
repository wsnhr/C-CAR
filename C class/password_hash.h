#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include "app_types.h"

/*
 * 为新密码生成随机盐和 SHA-256 哈希，并转换成可写入文本文件的
 * 十六进制字符串。返回 1=成功、0=失败。
 */
int password_hash_create(const char *plain_password, char salt_hex[PASSWORD_SALT_HEX_CAPACITY],
                         char hash_hex[PASSWORD_HASH_HEX_CAPACITY]);

/*
 * 校验密码：解码已保存的盐和哈希，对输入密码执行相同计算，
 * 再使用恒定时间比较结果。返回 1=匹配、0=不匹配或数据无效。
 */
int password_hash_matches(const char *plain_password, const char *salt_hex, const char *hash_hex);

#endif /* PASSWORD_HASH_H */
