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

// ---------- 车辆 ----------
typedef struct {
    char plate[10];      // 车牌号
    char brand[20];      // 品牌
    char model[20];      // 型号
    char owner[20];      // 车主（存用户名）
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


// ---------- 文件管理函数 ----------
int save_users(const AppContext* ctx);
int load_users(AppContext* ctx);
#endif
