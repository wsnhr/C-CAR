#define _CRT_SECURE_NO_WARNINGS
#include "insurance.h"
#include "app_utils.h"
#include "car.h"
#include "user.h"

#include <string.h>

/*
 * insurance.c —— 保单、理赔、金额计算和权限控制
 *
 * 数据关联链为 Car.plate -> Policy.plate -> Claim.policy_id。这里没有数据库
 * 自动维护外键，所以新增前必须手工查找关联对象，删除时也要主动级联清理。
 * 所有会成功/失败的业务操作都统一使用 1=成功、0=失败，并在公开的
 * 名称以 _for_current_user 结尾的公开接口负责执行身份和所有权检查。
 */

/*
 * 扫描所有有效保单，根据生效日期、失效日期和今天的先后关系重新计算 active。
 *
 * &p->end_date 取得嵌套 Date 成员的地址，交给日期比较函数读取。
 */
void update_policy_active_status(AppContext *ctx)
{
    Date today;
    int i;

    if (!ctx)
        return;
    today = date_today();
    for (i = 0; i < ctx->policy_count; ++i)
    {
        Policy *policy = &ctx->policies[i];
        if (!date_is_valid(&policy->start_date) || !date_is_valid(&policy->end_date))
            continue; /* 兼容没有日期字段的旧数据。 */
        policy->active = date_compare(&policy->start_date, &today) <= 0 &&
                         date_compare(&policy->end_date, &today) >= 0;
    }
}

/* 计算车辆年龄；未来年份被限制为 0，避免产生负数年龄。 */
static int car_age_years(const Car *car)
{
    Date today;
    int age;

    if (!car)
        return 0;
    if (car->purchase_date.year <= 0)
        return 0;
    today = date_today();
    age = today.year - car->purchase_date.year;
    if (age < 0)
        age = 0;
    return age;
}

/*
 计算保费的示例公式（年保费）
 说明（示例，便于替换）：
 - 基础费率 base_rate = 2.0% 的车价（即 0.02 * price）
 - 年龄因子 age_factor：新车（0年）系数 1.0，随着年龄增加保费可能下降，
   age_factor = max(0.6, 1.0 - age * 0.03) （每年减少3%，下限0.6）
 - 违章因子 violation_factor：根据违章严重度增加保费
   VIOLATION_NONE => 1.0
   VIOLATION_VERY_MINOR => 1.05
   VIOLATION_MINOR => 1.10
   VIOLATION_MODERATE => 1.25
   VIOLATION_RELATIVELY_SERIOUS => 1.5
   VIOLATION_SERIOUS => 2.0
 - 最终 premium = purchase_price * base_rate * age_factor * violation_factor
 - 最小保费最低为 100 元（示例）
*/
/* car 只用于读取，因此参数使用 const Car*；无有效车辆时返回 0.0。 */
double calculate_premium_for_car(const Car *car)
{
    if (!car)
        return 0.0;
    double price = car->purchase_price;
    if (price <= 0.0)
        price = 20000.0;           // 如果未知则取默认估计（示例）
    const double base_rate = 0.02; // 2%
    int age = car_age_years(car);
    double age_factor = 1.0 - age * 0.03;
    if (age_factor < 0.6)
        age_factor = 0.6;

    double violation_factor = 1.0;
    switch (car->violation)
    {
    case VIOLATION_VERY_MINOR:
        violation_factor = 1.05;
        break;
    case VIOLATION_MINOR:
        violation_factor = 1.10;
        break;
    case VIOLATION_MODERATE:
        violation_factor = 1.25;
        break;
    case VIOLATION_RELATIVELY_SERIOUS:
        violation_factor = 1.5;
        break;
    case VIOLATION_SERIOUS:
        violation_factor = 2.0;
        break;
    default:
        violation_factor = 1.0;
        break;
    }

    double premium = price * base_rate * age_factor * violation_factor;
    if (premium < 100.0)
        premium = 100.0;
    return premium;
}

/*
 计算理赔可批准金额的示例公式：
 - coverage_limit = policy->coverage_limit （通常基于购车价的比例）
 - request_amount = 申请金额
 - 基础赔付比例 base_ratio = 1.0
 - 违章会降低赔付比例：每一级违章降低 5% ，严重违章降低更多
 - 年龄降低赔付：老旧车辆在理赔时替换/修复价值低，按年降低比例
 - 最终 payout = min(request_amount, coverage_limit) * payout_ratio
 - payout_ratio 范围 [0.2, 1.0]
*/
/* 先按保额截断申请金额，再叠加车龄和违章系数，返回建议批准金额。 */
double calculate_payout_for_claim(const Policy *policy, const Car *car, double request_amount)
{
    if (!policy || !car || request_amount <= 0.0)
        return 0.0;

    double limit = policy->coverage_limit;
    if (limit <= 0.0)
    {
        // 若保单没有设定限额，使用车价的 80% 作为默认保额
        limit = car->purchase_price * 0.8;
        if (limit <= 0.0)
            limit = 5000.0;
    }

    double payout_base = request_amount;
    if (payout_base > limit)
        payout_base = limit;

    // 违章影响
    double violation_penalty = 0.0;
    switch (car->violation)
    {
    case VIOLATION_VERY_MINOR:
        violation_penalty = 0.05;
        break;
    case VIOLATION_MINOR:
        violation_penalty = 0.10;
        break;
    case VIOLATION_MODERATE:
        violation_penalty = 0.20;
        break;
    case VIOLATION_RELATIVELY_SERIOUS:
        violation_penalty = 0.35;
        break;
    case VIOLATION_SERIOUS:
        violation_penalty = 0.6;
        break;
    default:
        violation_penalty = 0.0;
        break;
    }

    int age = car_age_years(car);
    double age_penalty = age * 0.02; // 每年降低2%
    if (age_penalty > 0.6)
        age_penalty = 0.6; // 年龄惩罚上限

    double payout_ratio = 1.0 - violation_penalty - age_penalty;
    if (payout_ratio < 0.2)
        payout_ratio = 0.2; // 最低20%可以赔付（示例）
    if (payout_ratio > 1.0)
        payout_ratio = 1.0;

    double payout = payout_base * payout_ratio;
    return payout;
}

/* ---------- 保单与理赔管理实现 ---------- */

/* 内部查找工具：返回保单数组下标，未找到返回 -1。 */
static int find_policy_index(const AppContext *ctx, const char *policy_id)
{
    if (!ctx || !policy_id)
        return -1;
    for (int i = 0; i < ctx->policy_count; ++i)
    {
        if (strcmp(ctx->policies[i].policy_id, policy_id) == 0)
            return i;
    }
    return -1;
}

/* 内部查找工具：返回理赔数组下标，未找到返回 -1。 */
static int find_claim_index(const AppContext *ctx, const char *claim_id)
{
    if (!ctx || !claim_id)
        return -1;
    for (int i = 0; i < ctx->claim_count; ++i)
    {
        if (strcmp(ctx->claims[i].claim_id, claim_id) == 0)
            return i;
    }
    return -1;
}

/* 删除一个理赔数组元素，并把后续元素左移填补空位。 */
static void remove_claim_at(AppContext *ctx, int index)
{
    int i;

    for (i = index; i < ctx->claim_count - 1; ++i)
        ctx->claims[i] = ctx->claims[i + 1];
    --ctx->claim_count;
    memset(&ctx->claims[ctx->claim_count], 0, sizeof(ctx->claims[0]));
}

/* 删除保单前先删除所有引用该保单号的理赔，维持数据关联完整性。 */
static void remove_policy_at(AppContext *ctx, int index)
{
    int i;

    for (i = 0; i < ctx->claim_count;)
    {
        if (strcmp(ctx->claims[i].policy_id, ctx->policies[index].policy_id) == 0)
            remove_claim_at(ctx, i);
        else
            ++i;
    }

    for (i = index; i < ctx->policy_count - 1; ++i)
        ctx->policies[i] = ctx->policies[i + 1];
    --ctx->policy_count;
    memset(&ctx->policies[ctx->policy_count], 0, sizeof(ctx->policies[0]));
}

/*
 * 添加保单的底层实现。terms 把产品说明、日期、保额比例和保费倍数作为一组参数传入；
 * 函数会校验重复保单号、关联车辆和日期范围，然后计算保额与保费。
 */
static int add_policy(AppContext *ctx, const char *policy_id, const char *plate, const char *owner,
                      const PolicyTerms *terms)
{
    Car *car;
    Policy *policy;
    double limit;

    if (!ctx || !validate_string_len(policy_id, (int)sizeof(ctx->policies[0].policy_id)) ||
        !validate_plate(plate) ||
        !validate_string_len(owner, (int)sizeof(ctx->policies[0].owner)) || !terms ||
        !validate_string_len(terms->description, (int)sizeof(ctx->policies[0].coverage_desc)) ||
        !date_is_valid(&terms->start_date) || !date_is_valid(&terms->end_date) ||
        date_compare(&terms->start_date, &terms->end_date) > 0 || terms->coverage_ratio <= 0.0 ||
        terms->premium_multiplier <= 0.0)
        return 0;
    if (ctx->policy_count >= MAX_POLICIES || find_policy_index(ctx, policy_id) != -1)
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    policy = &ctx->policies[ctx->policy_count];
    memset(policy, 0, sizeof(*policy));
    copy_text(policy->policy_id, sizeof(policy->policy_id), policy_id);
    copy_text(policy->plate, sizeof(policy->plate), plate);
    copy_text(policy->owner, sizeof(policy->owner), owner);
    copy_text(policy->coverage_desc, sizeof(policy->coverage_desc), terms->description);
    policy->start_date = terms->start_date;
    policy->end_date = terms->end_date;

    limit = car->purchase_price * terms->coverage_ratio;
    policy->coverage_limit = limit > 0.0 ? limit : 5000.0;
    policy->premium = calculate_premium_for_car(car) * terms->premium_multiplier;
    {
        Date today = date_today();
        policy->active = date_compare(&policy->start_date, &today) <= 0 &&
                         date_compare(&policy->end_date, &today) >= 0;
    }

    ++ctx->policy_count;
    return 1;
}

/*
 * 按保单号删除。remove_policy_at 会先删除关联理赔，再前移保单数组元素。
 * 此底层函数不判断当前用户，权限由包装函数负责。
 */
static int remove_policy(AppContext *ctx, const char *policy_id)
{
    if (!ctx || !policy_id)
        return 0;
    int idx = find_policy_index(ctx, policy_id);
    if (idx == -1)
        return 0;
    remove_policy_at(ctx, idx);
    return 1;
}

/* 返回 AppContext 数组内部的保单地址；未找到返回 NULL，不需要 free。 */
static Policy *find_policy(AppContext *ctx, const char *policy_id)
{
    if (!ctx || !policy_id)
        return NULL;
    int idx = find_policy_index(ctx, policy_id);
    if (idx == -1)
        return NULL;
    return &ctx->policies[idx];
}

/*
 * 添加理赔：检查保单和车辆关联，计算 approved_amount，并将初始状态
 * 设为 CLAIM_PENDING。这个金额是按业务公式计算的建议值，仍需管理员审核。
 */
static int add_claim(AppContext *ctx, const char *claim_id, const char *policy_id,
                     const char *claimant, const char *description, double request_amount)
{
    if (!ctx || !validate_string_len(claim_id, (int)sizeof(ctx->claims[0].claim_id)) ||
        !validate_string_len(policy_id, (int)sizeof(ctx->claims[0].policy_id)) ||
        !validate_string_len(claimant, (int)sizeof(ctx->claims[0].claimant)) ||
        !validate_string_len(description, (int)sizeof(ctx->claims[0].description)) ||
        request_amount <= 0.0 || request_amount > 1e9)
        return 0;
    if (ctx->claim_count >= MAX_CLAIMS)
        return 0;
    if (find_claim_index(ctx, claim_id) != -1)
        return 0;

    Policy *p = find_policy(ctx, policy_id);
    if (!p || !p->active)
        return 0;

    Car *car = find_car(ctx, p->plate);
    if (!car)
        return 0;

    Claim *c = &ctx->claims[ctx->claim_count];
    memset(c, 0, sizeof(*c));
    copy_text(c->claim_id, sizeof(c->claim_id), claim_id);
    copy_text(c->policy_id, sizeof(c->policy_id), policy_id);
    copy_text(c->plate, sizeof(c->plate), p->plate);
    copy_text(c->claimant, sizeof(c->claimant), claimant);
    copy_text(c->description, sizeof(c->description), description);

    c->request_amount = request_amount;
    c->approved_amount = calculate_payout_for_claim(p, car, request_amount);
    c->status = CLAIM_PENDING;
    c->claim_date = date_today();

    ctx->claim_count++;
    return 1;
}

/* 返回数组内部理赔对象的地址；未找到返回 NULL。 */
static Claim *find_claim(AppContext *ctx, const char *claim_id)
{
    if (!ctx || !claim_id)
        return NULL;
    int idx = find_claim_index(ctx, claim_id);
    if (idx == -1)
        return NULL;
    return &ctx->claims[idx];
}

/* 只有待审核理赔可以进入“已通过”或“已驳回”状态。 */
static int review_claim(AppContext *ctx, const char *claim_id, int approve)
{
    Claim *claim = find_claim(ctx, claim_id);

    if (!claim || claim->status != CLAIM_PENDING)
        return 0;
    claim->status = approve ? CLAIM_APPROVED : CLAIM_REJECTED;
    return 1;
}

/*
 * 结案理赔：只有已审核通过的理赔可以结案。这里只修改业务状态，
 * 并不真的发起资金转账。
 */
static int settle_claim(AppContext *ctx, const char *claim_id)
{
    Claim *claim = find_claim(ctx, claim_id);

    if (!claim || claim->status != CLAIM_APPROVED)
        return 0;
    claim->status = CLAIM_SETTLED;
    return 1;
}

/* ================= 当前用户可见性与权限控制 ================= */
/*
 * 收集可见保单下标。管理员可见全部；普通用户只见 owner 等于自己的保单。
 * count < max_count 同时保护调用者的 indices 数组不被写越界。
 */
int collect_visible_policy_indices(const AppContext *ctx, int *indices, int max_count)
{
    int i, count = 0;

    if (!ctx || !indices || max_count <= 0)
        return 0;

    for (i = 0; i < ctx->policy_count && count < max_count; i++)
    {
        if (app_is_admin(ctx) || strcmp(ctx->policies[i].owner, ctx->current_user) == 0)
            indices[count++] = i;
    }

    return count;
}
/* 收集可见理赔下标；普通用户依据 claimant 字段过滤。 */
int collect_visible_claim_indices(const AppContext *ctx, int *indices, int max_count)
{
    int i, count = 0;

    if (!ctx || !indices || max_count <= 0)
        return 0;

    for (i = 0; i < ctx->claim_count && count < max_count; i++)
    {
        if (app_is_admin(ctx) || strcmp(ctx->claims[i].claimant, ctx->current_user) == 0)
            indices[count++] = i;
    }

    return count;
}
/* 权限感知的保单查找：存在但无权限时也返回 NULL。 */
const Policy *find_visible_policy(const AppContext *ctx, const char *policy_id)
{
    int i;

    if (!ctx || !policy_id)
        return NULL;

    for (i = 0; i < ctx->policy_count; i++)
    {
        if (strcmp(ctx->policies[i].policy_id, policy_id) == 0)
        {
            if (app_is_admin(ctx) || strcmp(ctx->policies[i].owner, ctx->current_user) == 0)
                return &ctx->policies[i];
            return NULL;
        }
    }

    return NULL;
}

/* 权限感知的理赔查找：管理员可查全部，普通用户只能查本人理赔。 */
const Claim *find_visible_claim(const AppContext *ctx, const char *claim_id)
{
    int i;

    if (!ctx || !claim_id)
        return NULL;
    for (i = 0; i < ctx->claim_count; ++i)
    {
        if (strcmp(ctx->claims[i].claim_id, claim_id) != 0)
            continue;
        if (app_is_admin(ctx) || strcmp(ctx->claims[i].claimant, ctx->current_user) == 0)
            return &ctx->claims[i];
        return NULL;
    }
    return NULL;
}
/*
 * 安全添加保单。管理员指定的 owner 必须存在且与车辆所有者一致；普通用户
 * 只能为自己的车辆投保。底层和包装层都使用 1=成功、0=失败。
 */
int add_policy_for_current_user(AppContext *ctx, const char *policy_id, const char *plate,
                                const char *requested_owner, const PolicyTerms *terms)
{
    Car *car;
    char owner[20];

    if (!ctx || !policy_id || !plate || !terms)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    if (app_is_admin(ctx))
    {
        if (!requested_owner || !user_exists(ctx, requested_owner))
            return 0;
        if (strcmp(requested_owner, car->owner) != 0)
            return 0;
        copy_text(owner, sizeof(owner), requested_owner);
    }
    else
    {
        if (strcmp(car->owner, ctx->current_user) != 0)
            return 0;
        copy_text(owner, sizeof(owner), ctx->current_user);
    }

    return add_policy(ctx, policy_id, plate, owner, terms);
}

/* 安全删除保单：登录、存在性和所有权检查全部通过后才调用底层删除。 */
int remove_policy_for_current_user(AppContext *ctx, const char *policy_id)
{
    Policy *policy;

    if (!ctx || !policy_id)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    policy = find_policy(ctx, policy_id);
    if (!policy)
        return 0;

    if (!app_is_admin(ctx) && strcmp(policy->owner, ctx->current_user) != 0)
        return 0;

    return remove_policy(ctx, policy_id);
}
/*
 * 安全提交理赔。普通用户只能针对自己的保单申请；管理员可代已存在用户
 * 提交。真正的关联检查和批准金额计算由 add_claim 完成。
 */
int add_claim_for_current_user(AppContext *ctx, const char *claim_id, const char *policy_id,
                               const char *requested_claimant, const char *description,
                               double request_amount)
{
    Policy *policy;
    char claimant[20];

    if (!ctx || !claim_id || !policy_id || !description)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    policy = find_policy(ctx, policy_id);
    if (!policy)
        return 0;

    if (app_is_admin(ctx))
    {
        if (!requested_claimant || !user_exists(ctx, requested_claimant))
            return 0;
        copy_text(claimant, sizeof(claimant), requested_claimant);
    }
    else
    {
        if (strcmp(policy->owner, ctx->current_user) != 0)
            return 0;
        copy_text(claimant, sizeof(claimant), ctx->current_user);
    }

    return add_claim(ctx, claim_id, policy_id, claimant, description, request_amount);
}
/* 安全审核：只有管理员可以审核，底层函数还会阻止重复审核。 */
int review_claim_for_current_user(AppContext *ctx, const char *claim_id, int approve)
{
    if (!ctx || !claim_id || !app_is_admin(ctx))
        return 0;
    return review_claim(ctx, claim_id, approve);
}

/* 安全结案：只有管理员可以把已审核通过的理赔设为已结案。 */
int settle_claim_for_current_user(AppContext *ctx, const char *claim_id)
{
    if (!ctx || !claim_id || !app_is_admin(ctx))
        return 0;
    return settle_claim(ctx, claim_id);
}

/* 删除某辆车的所有理赔和保单，供车辆模块维护关联完整性。 */
int remove_insurance_for_plate(AppContext *ctx, const char *plate)
{
    int i;

    if (!ctx || !plate)
        return 0;

    for (i = 0; i < ctx->claim_count;)
    {
        if (strcmp(ctx->claims[i].plate, plate) == 0)
            remove_claim_at(ctx, i);
        else
            ++i;
    }
    for (i = 0; i < ctx->policy_count;)
    {
        if (strcmp(ctx->policies[i].plate, plate) == 0)
            remove_policy_at(ctx, i);
        else
            ++i;
    }
    return 1;
}

/* 清理指定用户拥有/提交的剩余保险记录，供用户级联删除使用。 */
int remove_insurance_for_user(AppContext *ctx, const char *username)
{
    int i;

    if (!ctx || !username)
        return 0;

    for (i = 0; i < ctx->policy_count;)
    {
        if (strcmp(ctx->policies[i].owner, username) == 0)
            remove_policy_at(ctx, i);
        else
            ++i;
    }
    for (i = 0; i < ctx->claim_count;)
    {
        if (strcmp(ctx->claims[i].claimant, username) == 0)
            remove_claim_at(ctx, i);
        else
            ++i;
    }
    return 1;
}
