// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 容量上限
#define MAX_USERS 100
#define MAX_CARS  100

// ---------- 用户 ----------
typedef struct {
    char username[20];
    char password[20];
} User;

// ---------- 违章等级枚举（与车牌绑定）----------
// 无、较轻微、轻微、中等、较严重、严重
typedef enum {
    VIOLATION_NONE = 0,
    VIOLATION_VERY_MINOR,
    VIOLATION_MINOR,
    VIOLATION_MODERATE,
    VIOLATION_RELATIVELY_SERIOUS,
    VIOLATION_SERIOUS
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

// ---------- 总数据容器 ----------
typedef struct {
    User users[MAX_USERS];
    int  user_count;

    Car  cars[MAX_CARS];
    int  car_count;

    char current_user[20];  // 当前登录的用户名
} AppContext;

// ---------- 车辆管理函数原型 ----------
// 初始化应用上下文
void init_app(AppContext* ctx);

// 添加车辆：成功返回0，失败返回-1（已存在或已满或参数错误）
int add_car(AppContext* ctx, const char* plate, const char* brand, const char* model, const char* owner);

// 删除车辆（按车牌号）：成功返回0，失败返回-1
int remove_car(AppContext* ctx, const char* plate);

// 查找车辆索引，找不到返回-1
int find_car_index(const AppContext* ctx, const char* plate);

// 查找车辆指针，找不到返回NULL
Car* find_car(AppContext* ctx, const char* plate);

// 修改车辆信息（按车牌号）：成功返回0，失败返回-1
int modify_car(AppContext* ctx, const char* plate, const char* new_brand, const char* new_model, const char* new_owner);

// 列出所有车辆
void list_cars(const AppContext* ctx);
// ---------- 保单管理函数原型 ----------
//增加保单：成功返回0，失败返回-1
int add_claim(AppContext* ctx, const char* claim_id, const char* policy_id, const char* description, double amount);
//删除保单：成功返回0，失败返回 - 1
int settle_claim(AppContext* ctx, const char* claim_id);
//查找保单索引，找不到返回-1
Claim* find_claim(AppContext* ctx, const char* claim_id);

// ---------- 文件管理函数 ----------
int save_users(const AppContext* ctx);
int load_users(AppContext* ctx);
int save_cars(const AppContext* ctx);
int load_cars(AppContext* ctx);

#endif
