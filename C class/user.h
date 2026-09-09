#ifndef USER_H
#define USER_H

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* 查询和身份判断均为只读函数，返回 1=是、0=否。 */
    int user_exists(const AppContext *ctx, const char *username);
    int app_is_logged_in(const AppContext *ctx);
    int app_is_admin(const AppContext *ctx);
    /* 实名信息只用于本地规则校验和找回密码，不代表已连接权威实名服务。 */
    int validate_real_name(const char *real_name);
    int validate_chinese_id_card(const char *id_card);
    /* 注册/登录/重置只修改内存，不保存文件；控制层负责持久化和日志。 */
    int register_user_account(AppContext *ctx, const char *username, const char *password,
                              const char *real_name, const char *id_card);
    int login_user_account(AppContext *ctx, const char *username, const char *password);
    int reset_user_password(AppContext *ctx, const char *username, const char *real_name,
                            const char *id_card, const char *new_password);
    void logout_current_user(AppContext *ctx);

    /* 仅管理员可调用；删除用户并清理车辆、保单、理赔，成功返回 1。 */
    int remove_user_cascade(AppContext *ctx, const char *username);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
