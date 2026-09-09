#ifndef STORAGE_H
#define STORAGE_H

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* save_* 全量重写文件；load_* 全量恢复数组；均返回 1=成功、0=失败。 */
    int save_users(const AppContext *ctx);
    int load_users(AppContext *ctx);
    int save_cars(const AppContext *ctx);
    int load_cars(AppContext *ctx);
    int save_policies(const AppContext *ctx);
    int load_policies(AppContext *ctx);
    int save_claims(const AppContext *ctx);
    int load_claims(AppContext *ctx);

    /* 以追加方式记录一次已经完成的控制层操作。 */
    void append_operation_log(const AppContext *ctx, const char *action, const char *target,
                              const char *details);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
