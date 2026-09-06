#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif
    /* 标准库头文件 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
//#include<Windows.h>
/* 系统容量上限 */
#define MAX_USERS    100
#define MAX_CARS     100
#define MAX_POLICIES 100
#define MAX_CLAIMS   200

// ---------- 用户 ----------
typedef struct {
    char username[20];
    /* 存储盐值和加盐哈希（十六进制字符串）
       salt:     16 字节随机盐 -> 32 个 hex 字符 + 结尾 NUL
       password: 32 字节哈希结果 -> 64 个 hex 字符 + 结尾 NUL
       注意：这里保存的是哈希值，不是明文密码 */
    char salt[33];
    char password[65];
} User;

// ---------- 违章等级枚举 ----------
typedef enum {
    VIOLATION_NONE = 0,
    VIOLATION_VERY_MINOR,
    VIOLATION_MINOR,
    VIOLATION_MODERATE,
    VIOLATION_RELATIVELY_SERIOUS,
    VIOLATION_SERIOUS
} ViolationLevel;

// ---------- 日期结构 ----------
typedef struct {
    int year;
    int month;
    int day;
} Date;

// ---------- 车辆 ----------
typedef struct {
    char plate[10];
    char brand[20];
    char model[20];
    char owner[20];
    ViolationLevel violation;
    Date purchase_date;
    double purchase_price;
} Car;

// ---------- 保单 ----------
typedef struct {
    char policy_id[24];
    char plate[10];
    char owner[20];
    int active;
    double coverage_limit;
    double premium;
    Date start_date;
    Date end_date;
    char coverage_desc[64];
} Policy;

/* 理赔单状态 */
#define CLAIM_PENDING   0   /* 待审核：用户已提交，等待管理员审核 */
#define CLAIM_APPROVED  1   /* 审核通过：管理员已审核，等待结案 */
#define CLAIM_SETTLED   2   /* 已结案：管理员已完成理赔结案 */
#define CLAIM_REJECTED  3   /* 审核驳回：管理员驳回了该申请 */

// ---------- 理赔 ----------
typedef struct {
    char claim_id[24];
    char policy_id[24];
    char plate[10];
    char claimant[20];
    char description[256];
    double request_amount;
    double approved_amount;
    /* 理赔状态：0=待审核 1=审核通过 2=已结案 3=审核驳回 */
    int status;
    Date claim_date;
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

    char current_user[20];
} AppContext;


/* ================= 初始化 ================= */
void init_app(AppContext* ctx);

/* ================= 车辆管理 ================= */
int add_car(AppContext* ctx, const char* plate, const char* brand,
        const char* model, const char* owner,
        ViolationLevel violation, Date purchase_date,
        double purchase_price);

int remove_car(AppContext* ctx, const char* plate);

int find_car_index(const AppContext* ctx, const char* plate);

Car* find_car(AppContext* ctx, const char* plate);  

int modify_car(AppContext* ctx, const char* plate,
        const char* new_brand, const char* new_model,
        const char* new_owner,
        ViolationLevel new_violation,
        Date new_purchase_date, double new_purchase_price);

void list_cars(const AppContext* ctx);

/* ================= 保单管理 ================= */

int add_policy(AppContext* ctx, const char* policy_id,
        const char* plate, const char* owner,
        const char* coverage_desc);

int remove_policy(AppContext* ctx, const char* policy_id);

Policy* find_policy(AppContext* ctx, const char* policy_id);

int modify_policy(AppContext* ctx, const char* policy_id,
            const char* new_desc, Date new_start,
            Date new_end, double new_limit);

void  list_policies(const AppContext* ctx);

/* ================= 理赔管理 ================= */
int add_claim(AppContext* ctx, const char* claim_id,
        const char* policy_id, const char* claimant,
        const char* description, double request_amount);

Claim* find_claim(AppContext* ctx, const char* claim_id);

/* 审核理赔（管理员）：approve=1 审核通过，approve=0 审核驳回 */
int review_claim(AppContext* ctx, const char* claim_id, int approve);
/* 结案理赔（管理员）：仅对审核通过的理赔单结案 */
int settle_claim(AppContext* ctx, const char* claim_id);

void list_claims(const AppContext* ctx);

/* ================= 用户管理 ================= */
int user_exists(AppContext* ctx, const char* username);
int register_user(AppContext* ctx);
int login_user(AppContext* ctx);

/* ================= 文件持久化 ================= */
int save_users(const AppContext* ctx);
int load_users(AppContext* ctx);
int save_cars(const AppContext* ctx);
int load_cars(AppContext* ctx);
int save_policies(const AppContext* ctx);
int load_policies(AppContext* ctx);
int save_claims(const AppContext* ctx);
int load_claims(AppContext* ctx);



/* 统一认证接口：供 GUI 和控制台复用 */
int app_is_logged_in(const AppContext* ctx);
int app_is_admin(const AppContext* ctx);
int register_user_account(AppContext* ctx, const char* username, const char* password);
int login_user_account(AppContext* ctx, const char* username, const char* password);
void logout_current_user(AppContext* ctx);


/* ================= 车辆可见性与权限控制 ================= */
/* 根据当前用户收集可见车辆下标 */
int collect_visible_car_indices(const AppContext* ctx, int only_mine, int* indices, int max_count);
/* 在当前用户可见范围内查找车辆 */
const Car* find_visible_car(const AppContext* ctx, const char* plate);
/* 面向当前用户的车辆操作接口 */
int add_car_for_current_user(AppContext* ctx, const char* plate, const char* brand,
    const char* model, const char* requested_owner,
    ViolationLevel violation, Date purchase_date, double purchase_price);
int modify_car_for_current_user(AppContext* ctx, const char* plate,
    const char* new_brand, const char* new_model,
    ViolationLevel new_violation, Date new_purchase_date, double new_purchase_price);
int remove_car_for_current_user(AppContext* ctx, const char* plate);

    

/* ================= 保单/理赔可见性与权限控制 ================= */
/* 根据当前用户收集可见保单和理赔下标 */
int collect_visible_policy_indices(const AppContext* ctx, int* indices, int max_count);
int collect_visible_claim_indices(const AppContext* ctx, int* indices, int max_count);
/* 在当前用户可见范围内查找保单/理赔 */
const Policy* find_visible_policy(const AppContext* ctx, const char* policy_id);
const Claim* find_visible_claim(const AppContext* ctx, const char* claim_id);

/* 面向当前用户的保单/理赔操作接口 */
int add_policy_for_current_user(AppContext* ctx, const char* policy_id,
    const char* plate, const char* requested_owner,
    const char* coverage_desc);
int remove_policy_for_current_user(AppContext* ctx, const char* policy_id);
int add_claim_for_current_user(AppContext* ctx, const char* claim_id,
    const char* policy_id, const char* requested_claimant,
    const char* description, double request_amount);
/* 以当前用户身份审核理赔（仅管理员） */
int review_claim_for_current_user(AppContext* ctx, const char* claim_id, int approve);
/* 以当前用户身份结案理赔（仅管理员） */
int settle_claim_for_current_user(AppContext* ctx, const char* claim_id);


/* ================= 计算函数（供 GUI/保险逻辑使用） ================= */
/* 根据车辆参数计算建议年保费（示例公式） */
double calculate_premium_for_car(const Car* car);
/* 根据保单与申请金额、车辆参数计算可批准的赔付金额（示例公式） */
double calculate_payout_for_claim(const Policy* policy, const Car* car, double request_amount);

/* ================= 操作日志 ================= */
/* 记录每次的增删改操作：action 简短标识, target 为对象 (如 车牌/保单号), details 为可选文本 */
void append_operation_log(const AppContext* ctx, const char* action, const char* target, const char* details);

/* ================= 级联删除与自动状态更新 ================= */
/* 级联删除：删除车辆时清理关联保单与理赔 */
int cascade_remove_policies_and_claims_for_plate(AppContext* ctx, const char* plate);
/* 级联删除：删除用户时清理其车辆/保单/理赔并删除用户记录 */
int cascade_remove_for_user(AppContext* ctx, const char* username);
/* 自动过期检查（根据保单 end_date 设置 active=0） */
void update_policy_active_status(AppContext* ctx);
/* 新增：返回本地当前日期（年/月/日），在 gui.cpp 中有实现 */
Date today_local(void);

/* ================= 密码哈希 / 加盐（在 user.c 中实现） ================= */
/* 生成随机盐（字节），成功返回 1 */
int generate_salt(unsigned char* salt, int len);
/* 计算加盐哈希：SHA256(salt || password) -> out（32 字节），成功返回 1 */
int hash_password(const char* password, const unsigned char* salt, int salt_len, unsigned char out[32]);
/* 将二进制数据编码为十六进制字符串（不带 0x），out 缓冲区至少需要 in_len*2+1 字节 */
void hex_encode(const unsigned char* in, int in_len, char* out_hex);
/* 将十六进制字符串解码为二进制，out_len 为输出缓冲区大小，返回解码出的字节数，出错返回 -1 */
int hex_decode(const char* hex, unsigned char* out, int out_len);
/* 常量时间比较（登录校验用），两者相等返回 1 */
int secure_compare(const unsigned char* a, const unsigned char* b, int len);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
