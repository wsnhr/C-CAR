#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

/*
 insurance.c
 实现保单与理赔管理，以及基于车辆参数（车价、购买年份、违章等级等）
 的示例保费与赔付计算公式。

 说明（设计要点）：
 - 保单（Policy）在加入时，会根据关联车辆信息自动计算建议保额与保费。
 - 理赔（Claim）在创建时，会根据保单、关联车辆与申请金额计算可批准的赔付（approved_amount），但只有在 settle_claim 被调用时才标记为已结案。
 - 这里给出的是示例性计算逻辑，便于在项目中替换为业务真实规则。
 - 所有函数都尽量返回 0 表示成功，-1 表示失败（参数/未找到等）。
 - 注：调用方应确保 ctx 已通过 init_app 初始化并且保存/加载函数与持久化逻辑在 file.c 中与新字段兼容。
*/

/* 辅助：获取当前年 */
static int current_year(void) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    return tmv.tm_year + 1900;
}

/* 辅助：计算车辆年龄（年），若 purchase_date.year==0 则返回0 */
static int car_age_years(const Car* car) {
    if (!car) return 0;
    if (car->purchase_date.year <= 0) return 0;
    int age = current_year() - car->purchase_date.year;
    if (age < 0) age = 0;
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
double calculate_premium_for_car(const Car* car) {
    if (!car) return 0.0;
    double price = car->purchase_price;
    if (price <= 0.0) price = 20000.0; // 如果未知则取默认估计（示例）
    const double base_rate = 0.02; // 2%
    int age = car_age_years(car);
    double age_factor = 1.0 - age * 0.03;
    if (age_factor < 0.6) age_factor = 0.6;

    double violation_factor = 1.0;
    switch (car->violation) {
    case VIOLATION_VERY_MINOR: violation_factor = 1.05; break;
    case VIOLATION_MINOR: violation_factor = 1.10; break;
    case VIOLATION_MODERATE: violation_factor = 1.25; break;
    case VIOLATION_RELATIVELY_SERIOUS: violation_factor = 1.5; break;
    case VIOLATION_SERIOUS: violation_factor = 2.0; break;
    default: violation_factor = 1.0; break;
    }

    double premium = price * base_rate * age_factor * violation_factor;
    if (premium < 100.0) premium = 100.0;
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
double calculate_payout_for_claim(const Policy* policy, const Car* car, double request_amount) {
    if (!policy || !car || request_amount <= 0.0) return 0.0;

    double limit = policy->coverage_limit;
    if (limit <= 0.0) {
        // 若保单没有设定限额，使用车价的 80% 作为默认保额
        limit = car->purchase_price * 0.8;
        if (limit <= 0.0) limit = 5000.0;
    }

    double payout_base = request_amount;
    if (payout_base > limit) payout_base = limit;

    // 违章影响
    double violation_penalty = 0.0;
    switch (car->violation) {
    case VIOLATION_VERY_MINOR: violation_penalty = 0.05; break;
    case VIOLATION_MINOR: violation_penalty = 0.10; break;
    case VIOLATION_MODERATE: violation_penalty = 0.20; break;
    case VIOLATION_RELATIVELY_SERIOUS: violation_penalty = 0.35; break;
    case VIOLATION_SERIOUS: violation_penalty = 0.6; break;
    default: violation_penalty = 0.0; break;
    }

    int age = car_age_years(car);
    double age_penalty = age * 0.02; // 每年降低2%
    if (age_penalty > 0.6) age_penalty = 0.6; // 年龄惩罚上限

    double payout_ratio = 1.0 - violation_penalty - age_penalty;
    if (payout_ratio < 0.2) payout_ratio = 0.2; // 最低20%可以赔付（示例）
    if (payout_ratio > 1.0) payout_ratio = 1.0;

    double payout = payout_base * payout_ratio;
    return payout;
}

/* ---------- 保单与理赔管理实现 ---------- */

/* 查找保单索引 */
static int find_policy_index(const AppContext* ctx, const char* policy_id) {
    if (!ctx || !policy_id) return -1;
    for (int i = 0; i < ctx->policy_count; ++i) {
        if (strcmp(ctx->policies[i].policy_id, policy_id) == 0) return i;
    }
    return -1;
}

/* 查找理赔索引 */
static int find_claim_index(const AppContext* ctx, const char* claim_id) {
    if (!ctx || !claim_id) return -1;
    for (int i = 0; i < ctx->claim_count; ++i) {
        if (strcmp(ctx->claims[i].claim_id, claim_id) == 0) return i;
    }
    return -1;
}

/* 添加保单：
   - 需要关联已存在车辆（根据 plate 查找）
   - 自动设置 coverage_limit（示例：购车价的 80%）和 premium（调用 calculate_premium_for_car）
*/
int add_policy(AppContext* ctx, const char* policy_id, const char* plate, const char* owner, const char* coverage_desc) {
    if (!ctx || !policy_id || !plate || !owner) return -1;
    if (ctx->policy_count >= MAX_POLICIES) return -1;
    if (find_policy_index(ctx, policy_id) != -1) return -1; // 已存在

    Car* car = find_car(ctx, plate);
    if (!car) return -1; // 车牌必须存在

    Policy* p = &ctx->policies[ctx->policy_count];
    strncpy(p->policy_id, policy_id, sizeof(p->policy_id) - 1);
    p->policy_id[sizeof(p->policy_id) - 1] = '\0';
    strncpy(p->plate, plate, sizeof(p->plate) - 1);
    p->plate[sizeof(p->plate) - 1] = '\0';
    strncpy(p->owner, owner, sizeof(p->owner) - 1);
    p->owner[sizeof(p->owner) - 1] = '\0';
    p->active = 1;

    // coverage_limit：示例使用购车价的 80%
    double limit = car->purchase_price * 0.8;
    if (limit <= 0.0) limit = 5000.0; // 默认保额
    p->coverage_limit = limit;

    // premium：基于车辆计算出的建议年保费
    p->premium = calculate_premium_for_car(car);

    // 设置生效/失效日期为 0（可由调用方修改）
    p->start_date.year = p->start_date.month = p->start_date.day = 0;
    p->end_date.year = p->end_date.month = p->end_date.day = 0;

    if (coverage_desc) {
        strncpy(p->coverage_desc, coverage_desc, sizeof(p->coverage_desc) - 1);
        p->coverage_desc[sizeof(p->coverage_desc) - 1] = '\0';
    } else {
        p->coverage_desc[0] = '\0';
    }

    ctx->policy_count++;
    return 0;
}

/* 删除保单（按保单号） */
int remove_policy(AppContext* ctx, const char* policy_id) {
    if (!ctx || !policy_id) return -1;
    int idx = find_policy_index(ctx, policy_id);
    if (idx == -1) return -1;
    for (int i = idx; i < ctx->policy_count - 1; ++i) {
        ctx->policies[i] = ctx->policies[i + 1];
    }
    // 清理尾部
    ctx->policies[ctx->policy_count - 1].policy_id[0] = '\0';
    ctx->policies[ctx->policy_count - 1].plate[0] = '\0';
    ctx->policies[ctx->policy_count - 1].owner[0] = '\0';
    ctx->policies[ctx->policy_count - 1].active = 0;
    ctx->policies[ctx->policy_count - 1].coverage_limit = 0.0;
    ctx->policies[ctx->policy_count - 1].premium = 0.0;
    ctx->policy_count--;
    return 0;
}

/* 查找保单指针 */
Policy* find_policy(AppContext* ctx, const char* policy_id) {
    if (!ctx || !policy_id) return NULL;
    int idx = find_policy_index(ctx, policy_id);
    if (idx == -1) return NULL;
    return &ctx->policies[idx];
}

/* 修改保单（示例：修改描述、起始/结束日期与新的保额上限） */
int modify_policy(AppContext* ctx, const char* policy_id, const char* new_desc, Date new_start, Date new_end, double new_limit) {
    if (!ctx || !policy_id) return -1;
    Policy* p = find_policy(ctx, policy_id);
    if (!p) return -1;
    if (new_desc) {
        strncpy(p->coverage_desc, new_desc, sizeof(p->coverage_desc) - 1);
        p->coverage_desc[sizeof(p->coverage_desc) - 1] = '\0';
    }
    p->start_date = new_start;
    p->end_date = new_end;
    if (new_limit > 0.0) p->coverage_limit = new_limit;
    return 0;
}

/* 添加理赔：
   - 检查保单存在且激活
   - 关联车辆存在
   - 计算 approved_amount（但不自动支付）
*/
int add_claim(AppContext* ctx, const char* claim_id, const char* policy_id, const char* claimant, const char* description, double request_amount) {
    if (!ctx || !claim_id || !policy_id || !claimant || !description) return -1;
    if (ctx->claim_count >= MAX_CLAIMS) return -1;
    if (find_claim_index(ctx, claim_id) != -1) return -1; // 已有同名理赔单

    Policy* p = find_policy(ctx, policy_id);
    if (!p || !p->active) return -1; // 保单不存在或无效

    Car* car = find_car(ctx, p->plate);
    if (!car) return -1;

    Claim* c = &ctx->claims[ctx->claim_count];
    strncpy(c->claim_id, claim_id, sizeof(c->claim_id) - 1);
    c->claim_id[sizeof(c->claim_id) - 1] = '\0';
    strncpy(c->policy_id, policy_id, sizeof(c->policy_id) - 1);
    c->policy_id[sizeof(c->policy_id) - 1] = '\0';
    strncpy(c->plate, p->plate, sizeof(c->plate) - 1);
    c->plate[sizeof(c->plate) - 1] = '\0';
    strncpy(c->claimant, claimant, sizeof(c->claimant) - 1);
    c->claimant[sizeof(c->claimant) - 1] = '\0';
    strncpy(c->description, description, sizeof(c->description) - 1);
    c->description[sizeof(c->description) - 1] = '\0';

    c->request_amount = request_amount;
    c->approved_amount = calculate_payout_for_claim(p, car, request_amount);
    c->settled = 0;
    c->claim_date.year = c->claim_date.month = c->claim_date.day = 0;

    ctx->claim_count++;
    return 0;
}

/* 查找理赔指针 */
Claim* find_claim(AppContext* ctx, const char* claim_id) {
    if (!ctx || !claim_id) return NULL;
    int idx = find_claim_index(ctx, claim_id);
    if (idx == -1) return NULL;
    return &ctx->claims[idx];
}

/* 结案理赔：标记已结案（如果要模拟支付可在此处返回 approved_amount 或写入支付记录） */
int settle_claim(AppContext* ctx, const char* claim_id) {
    if (!ctx || !claim_id) return -1;
    int idx = find_claim_index(ctx, claim_id);
    if (idx == -1) return -1;
    ctx->claims[idx].settled = 1;
    // 这里可以记录实际支付金额、时间等。示例仅做逻辑标记。
    return 0;
}

/* 列出保单（便于调试） */
void list_policies(const AppContext* ctx) {
    if (!ctx) return;
    if (ctx->policy_count == 0) {
        printf("没有保单记录。\n");
        return;
    }
    printf("序号  保单号       车牌     投保人      保额(元)    年保费(元)  状态  描述\n");
    printf("-----------------------------------------------------------------------------\n");
    for (int i = 0; i < ctx->policy_count; ++i) {
        const Policy* p = &ctx->policies[i];
        printf("%-4d  %-12s  %-8s  %-10s  %-10.2f  %-10.2f  %-4s  %s\n",
               i+1, p->policy_id, p->plate, p->owner, p->coverage_limit, p->premium,
               p->active ? "在保" : "失效", p->coverage_desc);
    }
}

/* 列出理赔（便于调试） */
void list_claims(const AppContext* ctx) {
    if (!ctx) return;
    if (ctx->claim_count == 0) {
        printf("没有理赔记录。\n");
        return;
    }
    printf("序号  理赔号       保单号       车牌     申请(元)  批准(元)  状态  申请人  描述\n");
    printf("-----------------------------------------------------------------------------\n");
    for (int i = 0; i < ctx->claim_count; ++i) {
        const Claim* c = &ctx->claims[i];
        printf("%-4d  %-12s  %-12s  %-8s  %-8.2f  %-8.2f  %-4s  %-8s  %s\n",
               i+1, c->claim_id, c->policy_id, c->plate,
               c->request_amount, c->approved_amount,
               c->settled ? "已结" : "未结", c->claimant, c->description);
    }
}