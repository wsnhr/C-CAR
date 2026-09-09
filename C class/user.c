#define _CRT_SECURE_NO_WARNINGS
#include "user.h"
#include "app_utils.h"
#include "car.h"
#include "insurance.h"
#include "password_hash.h"

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
int register_user_account(AppContext *ctx, const char *username, const char *password)
{
    User *user;

    if (!ctx || !validate_string_len(username, (int)sizeof(ctx->users[0].username)) ||
        !validate_string_len(password, PASSWORD_INPUT_CAPACITY))
        return 0;

    if (ctx->user_count >= MAX_USERS)
        return 0;

    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    user = &ctx->users[ctx->user_count];
    memset(user, 0, sizeof(*user));
    if (!copy_text(user->username, sizeof(user->username), username) ||
        !password_hash_create(password, user->salt, user->password))
    {
        memset(user, 0, sizeof(*user));
        return 0;
    }
    ctx->user_count++;
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
