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
    if (!ctx || !username || !password || username[0] == '\0' || password[0] == '\0')
        return 0;

    if (ctx->user_count >= MAX_USERS)
        return 0;

    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    strcpy(ctx->users[ctx->user_count].username, username);
    strcpy(ctx->users[ctx->user_count].password, password);
    ctx->user_count++;
    save_users(ctx);
    return 1;
}
/* 统一登录接口：供 GUI 和控制台复用 */
int login_user_account(AppContext* ctx, const char* username, const char* password)
{
    int i;

    if (!ctx || !username || !password)
        return 0;

    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0) {
        strcpy(ctx->current_user, "admin");
        return 1;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) == 0 &&
            strcmp(ctx->users[i].password, password) == 0) {
            strcpy(ctx->current_user, username);
            return 1;
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
