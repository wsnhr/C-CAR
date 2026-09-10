#ifndef CAR_H
#define CAR_H

/*
 * car.h —— 车辆业务模块的公开接口
 *
 * GUI 应调用 *_for_current_user 接口，让 car.c 统一检查登录状态和所有权；
 * remove_car 是用户级联删除所需的模块间接口，不应由普通界面绕过权限调用。
 */

#include "app_types.h"

/*
 * 多条件查询使用的筛选结构体。
 * 每个字符串为空时表示“不限制该字段”；非空时使用“包含”关系匹配。
 * violation 为 CAR_VIOLATION_ANY 时不限制违章等级，否则应填写 ViolationLevel。
 */
#define CAR_VIOLATION_ANY (-1)
typedef struct
{
    char plate_keyword[10]; /* 车牌关键字，例如 "A-" 可匹配 "A-100"。 */
    char brand_keyword[20]; /* 品牌关键字。 */
    char model_keyword[20]; /* 型号关键字。 */
    char owner_keyword[20]; /* 车主用户名关键字。 */
    int violation;          /* -1=不限，0~5 对应 ViolationLevel。 */
} CarSearchCondition;

/* 车辆列表只提供三个有实际管理意义的排序字段。 */
typedef enum
{
    CAR_SORT_DEFAULT = 0,      /* 保持 cars 数组原来的登记顺序。 */
    CAR_SORT_VIOLATION,        /* 按违章等级枚举值排序。 */
    CAR_SORT_PURCHASE_DATE,    /* 按购入日期排序。 */
    CAR_SORT_PRICE             /* 按购入价格排序。 */
} CarSortField;

#ifdef __cplusplus
extern "C"
{
#endif

    /* 清空整个运行时上下文；main.c 在加载文件前调用一次。 *///???????????????????????????????????????????????????????????????????????????????????????????????????
    void init_app(AppContext *ctx);
    /* 返回数组内部车辆地址，未找到返回 NULL；调用者不能 free。 *///?????????????????????????????????????????????????????????
    Car *find_car(AppContext *ctx, const char *plate);

    /* 收集当前身份可见车辆的原数组下标，返回写入 indices 的数量。 */
    int collect_visible_car_indices(const AppContext *ctx, int only_mine, int *indices,
                                    int max_count);
    /*
     * 收集同时满足多个条件的车辆下标。多个非空条件之间是“并且”关系；
     * 普通用户无论 only_mine 的值为何，都只能得到自己的车辆。
     */
    int collect_matching_car_indices(const AppContext *ctx, int only_mine,
                                     const CarSearchCondition *condition, int *indices,
                                     int max_count);
    /* 只重新排列 indices，不改变 ctx->cars 和 cars.txt 的原始顺序。 */
    void sort_car_indices(const AppContext *ctx, int *indices, int count, CarSortField field,
                          int ascending);
    /* 权限感知查询；存在但无权查看时同样返回 NULL。 *///????????????????????????????????????????????
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

    /* 用户级联删除使用的模块间接口；它会同步删除车辆关联的保险数据。 *///?????????????????????????????????????????????????????????????????????????????????
    int remove_car(AppContext *ctx, const char *plate);

#ifdef __cplusplus
}
#endif

#endif /* CAR_H */
