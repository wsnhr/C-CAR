#ifndef STORAGE_H
#define STORAGE_H

/*
 * storage.h —— AppContext 与磁盘文本文件之间的转换接口
 *
 * 业务模块只修改内存；GUI 在操作成功后调用 save_* 持久化。程序启动时则
 * 调用 load_* 恢复数据。文件格式和文件名的实现细节统一放在 storage.c。
 */

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* save_* 全量重写文件；load_* 全量恢复数组；均返回 1=成功、0=失败。 */
    /* const AppContext* 表示保存过程只能读取上下文，不能修改它。 */
    int save_users(const AppContext *ctx);
    /* AppContext* 允许加载函数填写数组和对应 count。 */
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
