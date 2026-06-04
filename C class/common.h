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

#endif
