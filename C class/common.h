
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

/* 系统容量上限 */
#define MAX_USERS    100
#define MAX_CARS     100
#define MAX_POLICIES 100
#define MAX_CLAIMS   200

// ---------- 用户 ----------
typedef struct {
    char username[20];
    char password[20];
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

// ---------- 理赔 ----------
typedef struct {
    char claim_id[24];
    char policy_id[24];
    char plate[10];
    char claimant[20];
    char description[256];
    double request_amount;
    double approved_amount;
    int settled;
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
int settle_claim_for_current_user(AppContext* ctx, const char* claim_id);




#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
