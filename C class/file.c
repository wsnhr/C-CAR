//#define _CRT_SECURE_NO_WARNINGS
//#include "common.h"
//
//int save_users(const AppContext* ctx) {
//    FILE* fp;
//    int i;
//	int count;  // 保护变量，避免直接使用 ctx->user_count
//
//    fp = fopen("users.txt", "w");
//    if (fp == NULL) {
//        printf("用户数据保存失败！\n");
//        return 0;
//    }
//
//    //  核心：复制并校验 user_count
//    count = ctx->user_count;
//
//    if (count < 0) {
//        count = 0;
//    }
//
//    if (count > MAX_USERS) {
//        count = MAX_USERS;
//    }
//
//    // 用 count 写入，而不是 ctx->user_count
//    fprintf(fp, "%d\n", count);
//
//    for (i = 0; i < count; i++) {
//        fprintf(fp, "%s %s\n",
//            ctx->users[i].username,
//            ctx->users[i].password);
//    }
//
//    fclose(fp);
//    return 1;
//}
//
//int load_users(AppContext* ctx) {
//    FILE* fp;
//    int i;
//
//    fp = fopen("users.txt", "r");
//    if (fp == NULL) {
//        return 0;
//    }
//
//    if (fscanf(fp, "%d", &ctx->user_count) != 1) {
//        fclose(fp);
//        ctx->user_count = 0;
//        return 0;
//    }
//
//    if (ctx->user_count > MAX_USERS) {
//        ctx->user_count = MAX_USERS;
//    }
//
//    for (i = 0; i < ctx->user_count; i++) {
//        if (fscanf(fp, "%19s %19s",
//            ctx->users[i].username,
//            ctx->users[i].password) != 2) {
//
//            ctx->user_count = i;
//            break;
//        }
//    }
//
//    fclose(fp);
//    return 1;
//}
#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

/*
    追加保存一个新用户
    函数名仍然叫 save_users，避免其他文件找不到
*/
int save_users(const AppContext* ctx)
{
    FILE* fp;

    if (ctx->user_count <= 0) {
        return 0;
    }

    fp = fopen("users.txt", "a");   // 追加模式，不覆盖旧数据

    if (fp == NULL) {
        printf("用户数据保存失败！\n");
        return 0;
    }

    /*
        只保存最后一个新用户
    */
    fprintf(fp, "%s %s\n",
        ctx->users[ctx->user_count - 1].username,
        ctx->users[ctx->user_count - 1].password);

    fclose(fp);
    return 1;
}

/*
    读取所有用户
    不再读取第一行数量
*/
int load_users(AppContext* ctx)
{
    FILE* fp;

    fp = fopen("users.txt", "r");

    if (fp == NULL) {
        ctx->user_count = 0;
        return 0;
    }

    ctx->user_count = 0;

    while (ctx->user_count < MAX_USERS &&
        fscanf(fp, "%19s %19s",
            ctx->users[ctx->user_count].username,
            ctx->users[ctx->user_count].password) == 2)
    {
        ctx->user_count++;
    }

    fclose(fp);
    return 1;
}
int save_cars(const AppContext* ctx)
{
    FILE* fp;
    int i;

    fp = fopen("cars.txt", "w");   // 重写整个车辆文件

    if (fp == NULL) {
        printf("车辆数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->car_count; i++) {
        fprintf(fp, "%s %s %s %s\n",
            ctx->cars[i].plate,
            ctx->cars[i].brand,
            ctx->cars[i].model,
            ctx->cars[i].owner);
    }

    fclose(fp);
    return 1;
}
int load_cars(AppContext* ctx)
{
    FILE* fp;

    fp = fopen("cars.txt", "r");

    if (fp == NULL) {
        ctx->car_count = 0;
        return 0;
    }

    ctx->car_count = 0;

    while (ctx->car_count < MAX_CARS &&
        fscanf(fp, "%9s %19s %19s %19s",
            ctx->cars[ctx->car_count].plate,
            ctx->cars[ctx->car_count].brand,
            ctx->cars[ctx->car_count].model,
            ctx->cars[ctx->car_count].owner) == 4)
    {
        ctx->car_count++;
    }

    fclose(fp);
    return 1;
}
