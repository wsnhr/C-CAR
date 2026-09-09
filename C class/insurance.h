#ifndef INSURANCE_H
#define INSURANCE_H

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* 创建保单时需要的一组相关参数，避免用多个平行参数传递产品规则。 */
    typedef struct PolicyTerms
    {
        const char *description;   /* 指向产品说明字符串；函数只读取，不修改它。 */
        Date start_date;           /* 保单开始生效的日期。 */
        Date end_date;             /* 保单停止生效的日期，不能早于 start_date。 */
        double coverage_ratio;     /* 保额占购车价的比例，例如 0.8 表示 80%。 */
        double premium_multiplier; /* 在基础保费上乘的产品系数，例如 1.2。 */
    } PolicyTerms;

    /* 根据统一日期规则重新计算所有带合法日期的保单状态。 */
    void update_policy_active_status(AppContext *ctx);
    /* 以下两个函数是无副作用的金额计算。 */
    double calculate_premium_for_car(const Car *car);
    double calculate_payout_for_claim(const Policy *policy, const Car *car, double request_amount);

    /* 可见性函数只返回当前用户有权查看的数据。 */
    int collect_visible_policy_indices(const AppContext *ctx, int *indices, int max_count);
    int collect_visible_claim_indices(const AppContext *ctx, int *indices, int max_count);
    const Policy *find_visible_policy(const AppContext *ctx, const char *policy_id);
    const Claim *find_visible_claim(const AppContext *ctx, const char *claim_id);

    /* 面向 GUI 的操作统一使用 1=成功、0=失败，并在业务层执行权限检查。 */
    int add_policy_for_current_user(AppContext *ctx, const char *policy_id, const char *plate,
                                    const char *requested_owner, const PolicyTerms *terms);
    int remove_policy_for_current_user(AppContext *ctx, const char *policy_id);
    int add_claim_for_current_user(AppContext *ctx, const char *claim_id, const char *policy_id,
                                   const char *requested_claimant, const char *description,
                                   double request_amount);
    int review_claim_for_current_user(AppContext *ctx, const char *claim_id, int approve);
    int settle_claim_for_current_user(AppContext *ctx, const char *claim_id);

    /* 车辆/用户模块为维护跨实体关系而调用的模块间接口。 */
    int remove_insurance_for_plate(AppContext *ctx, const char *plate);
    int remove_insurance_for_user(AppContext *ctx, const char *username);

#ifdef __cplusplus
}
#endif

#endif /* INSURANCE_H */
