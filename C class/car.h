#ifndef CAR_H
#define CAR_H

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* 清空整个运行时上下文；main.c 在加载文件前调用一次。 */
    void init_app(AppContext *ctx);
    /* 返回数组内部车辆地址，未找到返回 NULL；调用者不能 free。 */
    Car *find_car(AppContext *ctx, const char *plate);

    /* 收集当前身份可见车辆的原数组下标，返回写入 indices 的数量。 */
    int collect_visible_car_indices(const AppContext *ctx, int only_mine, int *indices,
                                    int max_count);
    /* 权限感知查询；存在但无权查看时同样返回 NULL。 */
    const Car *find_visible_car(const AppContext *ctx, const char *plate);

    /* 以下面向 GUI 的操作全部统一使用 1=成功、0=失败。 */
    int add_car_for_current_user(AppContext *ctx, const char *plate, const char *brand,
                                 const char *model, const char *requested_owner,
                                 ViolationLevel violation, Date purchase_date,
                                 double purchase_price);
    int modify_car_for_current_user(AppContext *ctx, const char *plate, const char *new_brand,
                                    const char *new_model, ViolationLevel new_violation,
                                    Date new_purchase_date, double new_purchase_price);
    int remove_car_for_current_user(AppContext *ctx, const char *plate);

    /* 用户级联删除使用的模块间接口；它会同步删除车辆关联的保险数据。 */
    int remove_car(AppContext *ctx, const char *plate);

#ifdef __cplusplus
}
#endif

#endif /* CAR_H */
