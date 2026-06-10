#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
//检测用户名是否存在，存在返回1，不存在返回0
int user_exists(AppContext* ctx, const char* username) {
    int i;

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) == 0) {
            return 1;
        }
    }

    return 0;
}
//注册用户，成功返回1，失败返回0
int register_user(AppContext* ctx) {
    char username[20];
    char password[20];

    if (ctx->user_count >= MAX_USERS) {
        printf("用户数量已满，无法注册！\n");
        return 0;
    }

    printf("请输入用户名：");
    if (scanf("%19s", username) != 1) {
        printf("输入错误！\n");
        return 0;
    }

    if (user_exists(ctx, username)) {
        printf("用户名已存在，注册失败！\n");
        return 0;
    }

    printf("请输入密码：");
    if (scanf("%19s", password) != 1) {
        printf("输入错误！\n");
        return 0;
    }

    strcpy(ctx->users[ctx->user_count].username, username);
    strcpy(ctx->users[ctx->user_count].password, password);

    ctx->user_count++;
    save_users(ctx);
    printf("注册成功！\n");
    return 1;
}
//登录用户，成功返回1，失败返回0
int login_user(AppContext* ctx) {
    char username[20];
    char password[20];
    int i;

    printf("请输入用户名：");
    if (scanf("%19s", username) != 1) {
        printf("输入错误！\n");
        return 0;
    }

    printf("请输入密码：");
    if (scanf("%19s", password) != 1) {
        printf("输入错误！\n");
        return 0;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (strcmp(ctx->users[i].username, username) == 0 &&
            strcmp(ctx->users[i].password, password) == 0) {

            strcpy(ctx->current_user, username);
            printf("登录成功，欢迎 %s！\n", username);
            return 1;
        }
    }

    printf("用户名或密码错误！\n");
    return 0;
}
////认证菜单，提供注册和登录选项,但我没调用它，先留着
//void auth_menu(AppContext* ctx) {
//    int choice;
//
//    while (1) {
//        printf("\n===== 平安好车主系统 =====\n");
//        printf("1. 注册\n");
//        printf("2. 登录\n");
//        printf("0. 退出\n");
//        printf("请选择：");
//
//        if (scanf("%d", &choice) != 1) {
//            printf("输入错误！\n");
//            return;
//        }
//
//        switch (choice) {
//        case 1:
//            register_user(ctx);
//            break;
//        case 2:
//            login_user(ctx);
//            break;
//        case 0:
//            printf("退出系统。\n");
//            return;
//        default:
//            printf("无效选择！\n");
//        }
//    }
//}
////
