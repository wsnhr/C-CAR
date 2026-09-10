#ifndef USER_H
#define USER_H

/*
 * user.h —— 用户模块对外接口
 *
 * GUI 通过这些函数完成注册、登录和忘记密码处理，不应直接修改 User 数组。
 * 实现位于 user.c；密码哈希细节被继续封装在 password_hash.c 中。
 */

#include "app_types.h"

/* 带失败次数限制的登录接口需要区分三种结果，GUI 才能显示准确提示。 */
typedef enum
{
    LOGIN_RESULT_SUCCESS = 1,
    LOGIN_RESULT_WRONG_CREDENTIALS = 0,
    LOGIN_RESULT_LOCKED = -1
} LoginResult;

#ifdef __cplusplus
extern "C"
{
#endif

    /* 查询和身份判断均为只读函数，返回 1=是、0=否。 */
    int user_exists(const AppContext *ctx, const char *username);
    int app_is_logged_in(const AppContext *ctx);
    int app_is_admin(const AppContext *ctx);
    /* 实名信息只用于本地规则校验和忘记密码处理，不代表已连接权威实名服务。 */
    int validate_real_name(const char *real_name);
    int validate_chinese_id_card(const char *id_card);
    /* 注册/登录/重置只修改内存，不保存文件；控制层负责持久化和日志。 */
    /* 注册成功后 user_count 加 1，密码和实名组合都只保存盐与哈希。 */
    int register_user_account(AppContext *ctx, const char *username, const char *password,
                              const char *real_name, const char *id_card);
    /* 登录成功会把用户名写入 ctx->current_user。 */
    int login_user_account(AppContext *ctx, const char *username, const char *password);
    /*
     * 在普通登录验证外增加连续失败限制。输出参数可为 NULL；非 NULL 时分别
     * 接收剩余尝试次数和剩余锁定秒数。
     */
    LoginResult login_user_account_guarded(AppContext *ctx, const char *username,
                                           const char *password, int *remaining_attempts,
                                           int *remaining_seconds);
    /* 实名组合验证成功后生成全新的密码盐和哈希；内置管理员不能使用。 */
    int reset_user_password(AppContext *ctx, const char *username, const char *real_name,
                            const char *id_card, const char *new_password);
    /* 清空 current_user；不会删除用户数据。 */
    void logout_current_user(AppContext *ctx);

    /* 仅管理员可调用；删除用户并清理车辆、保单、理赔，成功返回 1。 */
    int remove_user_cascade(AppContext *ctx, const char *username);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
