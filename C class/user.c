#define _CRT_SECURE_NO_WARNINGS
#include "user.h"
#include "app_utils.h"
#include "car.h"
#include "insurance.h"
#include "password_hash.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * user.c —— 用户注册、登录和登录状态模块
 *
 * 本模块只负责账号数据和身份规则，不读取控制台，也不直接写文件。
 * current_user 为空表示未登录，固定用户名 admin 表示管理员。
 */

/* 顺序检查用户名。只遍历 users[0] 到 users[user_count-1]。 */
int user_exists(const AppContext *ctx, const char *username)
{
    int i;

    if (!ctx || !username)
        return 0;
    for (i = 0; i < ctx->user_count; i++)
    {
        if (strcmp(ctx->users[i].username, username) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static User *find_user(AppContext *ctx, const char *username)
{
    int i;
    if (!ctx || !username)
        return NULL;
    for (i = 0; i < ctx->user_count; ++i)
        if (strcmp(ctx->users[i].username, username) == 0)
            return &ctx->users[i];
    return NULL;
}

int validate_real_name(const char *real_name)
{
    const unsigned char *current;
    if (!validate_string_len(real_name, REAL_NAME_CAPACITY) || strlen(real_name) < 2)
        return 0;
    for (current = (const unsigned char *)real_name; *current; ++current)
        if (*current < 0x80 && !isalpha(*current))
            return 0;
    return 1;
}

int validate_chinese_id_card(const char *id_card)
{
    static const int weights[17] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
    static const char checks[] = "10X98765432";
    Date birth_date;
    int i, sum = 0;

    if (!id_card || strlen(id_card) != 18)
        return 0;
    for (i = 0; i < 17; ++i)
    {
        if (!isdigit((unsigned char)id_card[i]))
            return 0;
        sum += (id_card[i] - '0') * weights[i];
    }
    if (toupper((unsigned char)id_card[17]) != checks[sum % 11])
        return 0;
    if (strncmp(id_card, "000000", 6) == 0 || strncmp(id_card + 14, "000", 3) == 0)
        return 0;

    birth_date.year = (id_card[6] - '0') * 1000 + (id_card[7] - '0') * 100 +
                      (id_card[8] - '0') * 10 + (id_card[9] - '0');
    birth_date.month = (id_card[10] - '0') * 10 + (id_card[11] - '0');
    birth_date.day = (id_card[12] - '0') * 10 + (id_card[13] - '0');
    {
        Date today = date_today();
        return date_is_valid(&birth_date) && date_compare(&birth_date, &today) <= 0;
    }
}

static int make_identity_text(const char *real_name, const char *id_card, char *output,
                              size_t output_capacity)
{
    char normalized_id[ID_CARD_CAPACITY];
    int written;
    if (!output || !validate_real_name(real_name) || !validate_chinese_id_card(id_card))
        return 0;
    if (!copy_text(normalized_id, sizeof(normalized_id), id_card))
        return 0;
    normalized_id[17] = (char)toupper((unsigned char)normalized_id[17]);
    written = snprintf(output, output_capacity, "%s|%s", real_name, normalized_id);
    return written > 0 && (size_t)written < output_capacity;
}

/* ================= 统一认证与登录状态接口 ================= */
/*
 * 判断是否登录。&& 会短路：ctx 为 NULL 时不会继续访问 ctx->current_user，
 * 因而不会发生空指针访问。
 */
int app_is_logged_in(const AppContext *ctx)
{
    return ctx && ctx->current_user[0] != '\0';
}
/* 只有已登录且 current_user 与 "admin" 完全相等时才返回真。 */
int app_is_admin(const AppContext *ctx)
{
    return app_is_logged_in(ctx) && strcmp(ctx->current_user, "admin") == 0;
}
/*
 * 统一注册接口。依次校验指针、空串、容量、保留名和重复名，然后把数据
 * 写入 users[user_count]。密码先加随机盐并计算 SHA-256，结构体中只保存
 * 盐和哈希的十六进制文本。文件保存由 GUI 控制层统一负责。
 */
int register_user_account(AppContext *ctx, const char *username, const char *password,
                          const char *real_name, const char *id_card)
{
    User *user;
    char identity_text[REAL_NAME_CAPACITY + ID_CARD_CAPACITY + 1];

    if (!ctx || !validate_string_len(username, (int)sizeof(ctx->users[0].username)) ||
        !validate_string_len(password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;

    if (ctx->user_count >= MAX_USERS)
        return 0;

    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    user = &ctx->users[ctx->user_count];
    memset(user, 0, sizeof(*user));
    if (!copy_text(user->username, sizeof(user->username), username) ||
        !password_hash_create(password, user->salt, user->password) ||
        !password_hash_create(identity_text, user->identity_salt, user->identity_hash))
    {
        memset(user, 0, sizeof(*user));
        return 0;
    }
    ctx->user_count++;
    return 1;
}

int reset_user_password(AppContext *ctx, const char *username, const char *real_name,
                        const char *id_card, const char *new_password)
{
    User *user;
    char identity_text[REAL_NAME_CAPACITY + ID_CARD_CAPACITY + 1];
    char new_salt[PASSWORD_SALT_HEX_CAPACITY];
    char new_hash[PASSWORD_HASH_HEX_CAPACITY];

    if (!ctx || !username || strcmp(username, "admin") == 0 ||
        !validate_string_len(new_password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;
    user = find_user(ctx, username);
    if (!user || !user->identity_salt[0] || !user->identity_hash[0] ||
        !password_hash_matches(identity_text, user->identity_salt, user->identity_hash))
        return 0;
    if (!password_hash_create(new_password, new_salt, new_hash))
        return 0;
    copy_text(user->salt, sizeof(user->salt), new_salt);
    copy_text(user->password, sizeof(user->password), new_hash);
    return 1;
}
/*
 * 统一登录接口。管理员是内置特殊分支；普通用户先按用户名找到记录，
 * 再用同一盐值重新计算输入密码的哈希。哈希匹配后才设置 current_user。
 */
int login_user_account(AppContext *ctx, const char *username, const char *password)
{
    int i;

    if (!ctx || !username || !password)
        return 0;

    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0)
    {
        copy_text(ctx->current_user, sizeof(ctx->current_user), "admin");
        return 1;
    }

    for (i = 0; i < ctx->user_count; i++)
    {
        if (strcmp(ctx->users[i].username, username) == 0 &&
            password_hash_matches(password, ctx->users[i].salt, ctx->users[i].password))
        {
            copy_text(ctx->current_user, sizeof(ctx->current_user), username);
            return 1;
        }
    }

    return 0;
}
/* 把第一个字符设为 '\0'，即可让 current_user 成为空字符串。 */
void logout_current_user(AppContext *ctx)
{
    if (ctx)
        ctx->current_user[0] = '\0';
}

/*
 * 删除用户的业务事务：先删除其车辆及车辆保险，再清理剩余保险数据，最后
 * 从用户数组移除账号。函数不保存文件、不写日志，调用者统一处理这些副作用。
 */
int remove_user_cascade(AppContext *ctx, const char *username)
{
    int i;

    if (!ctx || !app_is_admin(ctx) || !username || strcmp(username, "admin") == 0 ||
        !user_exists(ctx, username))
        return 0;

    for (i = 0; i < ctx->car_count;)
    {
        if (strcmp(ctx->cars[i].owner, username) == 0)
            remove_car(ctx, ctx->cars[i].plate);
        else
            ++i;
    }
    remove_insurance_for_user(ctx, username);

    for (i = 0; i < ctx->user_count; ++i)
    {
        int j;
        if (strcmp(ctx->users[i].username, username) != 0)
            continue;
        for (j = i; j < ctx->user_count - 1; ++j)
            ctx->users[j] = ctx->users[j + 1];
        --ctx->user_count;
        memset(&ctx->users[ctx->user_count], 0, sizeof(ctx->users[0]));
        if (strcmp(ctx->current_user, username) == 0)
            logout_current_user(ctx);
        return 1;
    }
    return 0;
}
