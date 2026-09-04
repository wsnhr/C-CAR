#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
//检测用户名是否存在，存在返回1，不存在返回0
int user_exists(AppContext* ctx, const char* username) {
    int i;

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) == 0) {
            return 1;
        }
    }

    return 0;
}
//注册用户，成功返回1，失败返回0
int register_user(AppContext* ctx) {
    char username[20];
    char password[20];

    printf("请输入用户名：");
    if (scanf("%19s", username) != 1) {
        printf("输入错误\n");
        return 0;
    }

    printf("请输入密码：");
    if (scanf("%19s", password) != 1) {
        printf("输入错误\n");
        return 0;
    }

    if (register_user_account(ctx, username, password)) {
        printf("注册成功！\n");
        return 1;
    }

    if (ctx->user_count >= MAX_USERS)
        printf("用户数量已满，无法注册！\n");
    else if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        printf("用户名已存在，注册失败！\n");
    else
        printf("注册失败！\n");

    return 0;
}
//登录用户，成功返回1，失败返回0
int login_user(AppContext* ctx) {
    char username[20];
    char password[20];

    printf("请输入用户名：");
    if (scanf("%19s", username) != 1) {
        printf("输入错误\n");
        return 0;
    }

    printf("请输入密码：");
    if (scanf("%19s", password) != 1) {
        printf("输入错误\n");
        return 0;
    }

    if (login_user_account(ctx, username, password)) {
        printf("登录成功，欢迎 %s！\n", ctx->current_user);
        return 1;
    }

    printf("用户名或密码错误\n");
    return 0;
}
/* ================= 统一认证与登录状态接口 ================= */
/* 判断当前是否已登录 */
int app_is_logged_in(const AppContext* ctx)
{
    return ctx && ctx->current_user[0] != '\0';
}
/* 判断当前是否为管理员 */
int app_is_admin(const AppContext* ctx)
{
    return app_is_logged_in(ctx) && strcmp(ctx->current_user, "admin") == 0;
}
/* 统一注册接口：供 GUI 和控制台复用 */
int register_user_account(AppContext* ctx, const char* username, const char* password)
{
    unsigned char salt[16];
    unsigned char hash[32];
    User* u;

    if (!ctx || !username || !password || username[0] == '\0' || password[0] == '\0')
        return 0;

    if (ctx->user_count >= MAX_USERS)
        return 0;

    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    u = &ctx->users[ctx->user_count];

    strcpy(u->username, username);

    /* 生成随机盐，计算加盐 SHA-256 哈希，并以十六进制字符串保存 */
    generate_salt(salt, sizeof(salt));
    hash_password(password, salt, sizeof(salt), hash);

    hex_encode(salt, sizeof(salt), u->salt);
    hex_encode(hash, sizeof(hash), u->password);

    ctx->user_count++;
    save_users(ctx);
    return 1;
}
/* 统一登录接口：供 GUI 和控制台复用 */
int login_user_account(AppContext* ctx, const char* username, const char* password)
{
    int i;
    unsigned char salt[16];
    unsigned char hash[32];
    unsigned char stored_hash[32];

    if (!ctx || !username || !password)
        return 0;

    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0) {
        strcpy(ctx->current_user, "admin");
        return 1;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) == 0) {
            /* 取出该用户保存的盐和哈希，用同一把盐对输入的密码重新哈希后比较 */
            if (hex_decode(ctx->users[i].salt, salt, (int)sizeof(salt)) != (int)sizeof(salt))
                continue;
            if (hex_decode(ctx->users[i].password, stored_hash, (int)sizeof(stored_hash)) != (int)sizeof(stored_hash))
                continue;

            hash_password(password, salt, sizeof(salt), hash);

            if (secure_compare(hash, stored_hash, (int)sizeof(hash))) {
                strcpy(ctx->current_user, username);
                return 1;
            }
        }
    }

    return 0;
}
/* 清除当前登录用户 */
void logout_current_user(AppContext* ctx)
{
    if (ctx)
        ctx->current_user[0] = '\0';
}

/* ================= 密码哈希 / 加盐 ================= */
/* 加盐 SHA-256：hash = SHA256(盐 || 密码)。
   盐和哈希都以小写十六进制字符串保存，磁盘上不会出现明文密码。 */

/* SHA-256 上下文结构 */
typedef struct {
    unsigned int state[8];
    unsigned char buf[64];
    unsigned int curlen;         /* 缓冲区中当前暂存的字节数 */
    unsigned long long length;   /* 已经完整处理掉的字节数 */
} Sha256Ctx;

/* SHA-256 常量表 K */
static const unsigned int sha256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* 循环右移 n 位 */
#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* 压缩函数：处理一个 64 字节的分组 */
static void sha256_transform(Sha256Ctx* ctx, const unsigned char block[64])
{
    unsigned int w[64];
    unsigned int a, b, c, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned int)block[i * 4] << 24) |
               ((unsigned int)block[i * 4 + 1] << 16) |
               ((unsigned int)block[i * 4 + 2] << 8) |
               ((unsigned int)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        unsigned int s0 = ROTR32(w[i - 15], 7) ^ ROTR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = ROTR32(w[i - 2], 17) ^ ROTR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        unsigned int S1 = ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int S0 = ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);

        t1 = h + S1 + ch + sha256_K[i] + w[i];
        t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

/* 初始化 SHA-256 上下文 */
static void sha256_init(Sha256Ctx* ctx)
{
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->curlen = 0;
    ctx->length = 0;
}

/* 追加数据 */
static void sha256_update(Sha256Ctx* ctx, const unsigned char* data, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++) {
        ctx->buf[ctx->curlen++] = data[i];
        if (ctx->curlen == 64) {
            sha256_transform(ctx, ctx->buf);
            ctx->length += 64;
            ctx->curlen = 0;
        }
    }
}

/* 收尾：补齐填充并输出 32 字节摘要 */
static void sha256_final(Sha256Ctx* ctx, unsigned char out[32])
{
    unsigned long long bitlen = (ctx->length + (unsigned long long)ctx->curlen) * 8ULL;
    unsigned char pad = 0x80;
    int i;

    sha256_update(ctx, &pad, 1);

    while (ctx->curlen != 56) {
        unsigned char zero = 0x00;
        sha256_update(ctx, &zero, 1);
    }

    for (i = 0; i < 8; i++) {
        unsigned char byte = (unsigned char)(bitlen >> (56 - 8 * i));
        sha256_update(ctx, &byte, 1);
    }

    for (i = 0; i < 8; i++) {
        out[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

/* 生成随机盐 */
int generate_salt(unsigned char* salt, int len)
{
    static int seeded = 0;
    int i;

    if (!salt || len <= 0)
        return 0;

    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }

    for (i = 0; i < len; i++)
        salt[i] = (unsigned char)(rand() & 0xFF);

    return 1;
}

/* 计算加盐哈希：SHA256(salt || password) */
int hash_password(const char* password, const unsigned char* salt, int salt_len, unsigned char out[32])
{
    Sha256Ctx ctx;

    if (!password || !salt || salt_len <= 0 || !out)
        return 0;

    sha256_init(&ctx);
    sha256_update(&ctx, salt, (unsigned int)salt_len);
    sha256_update(&ctx, (const unsigned char*)password, (unsigned int)strlen(password));
    sha256_final(&ctx, out);
    return 1;
}

/* 单个十六进制字符转数值，非法字符返回 -1 */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* 二进制编码为十六进制字符串 */
void hex_encode(const unsigned char* in, int in_len, char* out_hex)
{
    static const char digits[] = "0123456789abcdef";
    int i;

    for (i = 0; i < in_len; i++) {
        out_hex[i * 2]     = digits[in[i] >> 4];
        out_hex[i * 2 + 1] = digits[in[i] & 0x0F];
    }
    out_hex[in_len * 2] = '\0';
}

/* 十六进制字符串解码为二进制 */
int hex_decode(const char* hex, unsigned char* out, int out_len)
{
    int i = 0;
    int n = 0;

    if (!hex || !out)
        return -1;

    while (hex[i] != '\0') {
        int hi, lo;

        if (hex[i + 1] == '\0')
            return -1; /* 十六进制字符个数为奇数 */

        hi = hex_val(hex[i]);
        lo = hex_val(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return -1;

        if (n >= out_len)
            return -1;

        out[n++] = (unsigned char)((hi << 4) | lo);
        i += 2;
    }

    return n;
}

/* 常量时间比较，相等返回 1 */
int secure_compare(const unsigned char* a, const unsigned char* b, int len)
{
    unsigned char diff = 0;
    int i;

    for (i = 0; i < len; i++)
        diff |= a[i] ^ b[i];

    return diff == 0;
}

