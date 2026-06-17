// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h> // 供实现中计算年

// 容量上限
#define MAX_USERS    100
#define MAX_CARS     100
#define MAX_POLICIES 100
#define MAX_CLAIMS   200

// ---------- 用户 ----------
typedef struct {
    char username[20];
    char password[20];
} User;

// ---------- 违章等级枚举（与车牌绑定）----------

typedef enum {
    VIOLATION_NONE = 0,  //无
    VIOLATION_VERY_MINOR,//较轻微
    VIOLATION_MINOR,//轻微
    VIOLATION_MODERATE,//中等
    VIOLATION_RELATIVELY_SERIOUS,//较严重
    VIOLATION_SERIOUS//严重
} ViolationLevel;

// ---------- 日期结构（购买时间）----------
typedef struct {
    int year;   // 年，例如 2023
    int month;  // 月，1-12
    int day;    // 日，1-31
} Date;

// ---------- 车辆 ----------
typedef struct {
    char plate[10];      // 车牌号
    char brand[20];      // 品牌
    char model[20];      // 型号
    char owner[20];      // 车主（存用户名）

    ViolationLevel violation; // 违章程度（枚举）
    Date purchase_date;       // 购买时间（年/月/日）
    double purchase_price;    // 购买时价格（元,人民币）
} Car;

// ---------- 保单（Policy） ----------
typedef struct {
    char policy_id[24];     // 保单号（唯一）
    char plate[10];         // 关联车牌
    char owner[20];         // 投保人（用户名）
    int active;             // 1=有效，0=失效/取消
    double coverage_limit;  // 保额上限（单位：元）
    double premium;         // 年保费（单位：元）
    Date start_date;        // 生效日期（可选使用）
    Date end_date;          // 失效日期（可选使用）
    char coverage_desc[64]; // 保障描述
} Policy;

// ---------- 理赔（Claim） ----------
typedef struct {
    char claim_id[24];      // 理赔单号（唯一）
    char policy_id[24];     // 关联保单号
    char plate[10];         // 关联车牌
    char claimant[20];      // 申请人用户名
    char description[256];  // 事故/理赔描述
    double request_amount;  // 申请金额
    double approved_amount; // 按公式计算出的可赔付金额（尚未/已支付）
    int settled;            // 0=未结案，1=已结案并支付
    Date claim_date;        // 申请日期（可选）
} Claim;

// ---------- 总数据容器 ----------
typedef struct {
    User users[MAX_USERS];
    int  user_count;

    Car  cars[MAX_CARS];
    int  car_count;

    Policy policies[MAX_POLICIES];
    int    policy_count;

    Claim claims[MAX_CLAIMS];
    int   claim_count;

    char current_user[20];  // 当前登录的用户名
} AppContext;

// ---------- 车辆管理函数原型 ----------
// 初始化应用上下文
void init_app(AppContext* ctx);

// 添加车辆：（请在实现中保持兼容）
// 旧签名保留，项目中如需可重载或替换为完整签名
int add_car(AppContext* ctx, const char* plate, const char* brand, const char* model, const char* owner,
    ViolationLevel violation, Date purchase_date, double purchase_price);

// 删除车辆（按车牌号）：成功返回0，失败返回-1
int remove_car(AppContext* ctx, const char* plate);

// 查找车辆索引，找不到返回-1
int find_car_index(const AppContext* ctx, const char* plate);

// 查找车辆指针，找不到返回NULL
Car* find_car(AppContext* ctx, const char* plate);

// 修改车辆信息（按车牌号）：成功返回0，失败返回-1
int modify_car(AppContext* ctx, const char* plate, const char* new_brand, const char* new_model, const char* new_owner,
    ViolationLevel new_violation, Date new_purchase_date, double new_purchase_price);

// 列出所有车辆
void list_cars(const AppContext* ctx);

// ---------- 保单/理赔管理函数原型 ----------
// 保单操作
int add_policy(AppContext* ctx, const char* policy_id, const char* plate, const char* owner, const char* coverage_desc);
int remove_policy(AppContext* ctx, const char* policy_id);
Policy* find_policy(AppContext* ctx, const char* policy_id);
int modify_policy(AppContext* ctx, const char* policy_id, const char* new_desc, Date new_start, Date new_end, double new_limit);

// 理赔操作
int add_claim(AppContext* ctx, const char* claim_id, const char* policy_id, const char* claimant, const char* description, double request_amount);
Claim* find_claim(AppContext* ctx, const char* claim_id);
int settle_claim(AppContext* ctx, const char* claim_id);

// 计算函数（可用于 UI 显示或内部自动计算）
// 根据车辆参数计算建议年保费（示例公式，含注释）
double calculate_premium_for_car(const Car* car);

// 根据保单与申请金额、车辆参数计算可批准的赔付金额（示例公式）
double calculate_payout_for_claim(const Policy* policy, const Car* car, double request_amount);

#endif
