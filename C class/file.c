#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

int save_users(const AppContext* ctx) {
    FILE* fp;
    int i;
	int count;  // 保护变量，避免直接使用 ctx->user_count

    fp = fopen("users.txt", "w");
    if (fp == NULL) {
        printf("用户数据保存失败！\n");
        return 0;
    }

    //  核心：复制并校验 user_count
    count = ctx->user_count;

    if (count < 0) {
        count = 0;
    }

    if (count > MAX_USERS) {
        count = MAX_USERS;
    }

    // 用 count 写入，而不是 ctx->user_count
    fprintf(fp, "%d\n", count);

    for (i = 0; i < count; i++) {
        fprintf(fp, "%s %s\n",
            ctx->users[i].username,
            ctx->users[i].password);
    }

    fclose(fp);
    return 1;
}

int load_users(AppContext* ctx) {
    FILE* fp;
    int i;

    fp = fopen("users.txt", "r");
    if (fp == NULL) {
        return 0;
    }

    if (fscanf(fp, "%d", &ctx->user_count) != 1) {
        fclose(fp);
        ctx->user_count = 0;
        return 0;
    }

    if (ctx->user_count > MAX_USERS) {
        ctx->user_count = MAX_USERS;
    }

    for (i = 0; i < ctx->user_count; i++) {
        if (fscanf(fp, "%19s %19s",
            ctx->users[i].username,
            ctx->users[i].password) != 2) {

            ctx->user_count = i;
            break;
        }
    }

    fclose(fp);
    return 1;
}