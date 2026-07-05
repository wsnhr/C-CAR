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
        fprintf(fp, "%s %s %s %s %d %d %d %d %.2f\n",
                ctx->cars[i].plate,
                ctx->cars[i].brand,
                ctx->cars[i].model,
                ctx->cars[i].owner,
                (int)ctx->cars[i].violation,
                ctx->cars[i].purchase_date.year,
                ctx->cars[i].purchase_date.month,
                ctx->cars[i].purchase_date.day,
                ctx->cars[i].purchase_price);
    }

    fclose(fp);
    return 1;
}

/* 加载车辆：兼容旧格式（如果旧文件只有前4个字段，则使用默认值） */
int load_cars(AppContext* ctx)
{
    FILE* fp;
    int read_count;

    fp = fopen("cars.txt", "r");
    if (fp == NULL) {
        ctx->car_count = 0;
        return 0;
    }

    ctx->car_count = 0;

    while (ctx->car_count < MAX_CARS) {
        Car tmp;
        int v;
        int y, m, d;
        double price;

        // 先尝试完整格式（9项：4字符串 + 4 ints + double）
        read_count = fscanf(fp, "%9s %19s %19s %19s %d %d %d %d %lf",
                            tmp.plate, tmp.brand, tmp.model, tmp.owner,
                            &v, &y, &m, &d, &price);
        if (read_count == 9) {
            tmp.violation = (v >= VIOLATION_NONE && v <= VIOLATION_SERIOUS) ? (ViolationLevel)v : VIOLATION_NONE;
            tmp.purchase_date.year = y; tmp.purchase_date.month = m; tmp.purchase_date.day = d;
            tmp.purchase_price = price;
            ctx->cars[ctx->car_count++] = tmp;
            continue;
        }

        // 如果不是完整格式，尝试老格式（仅4字符串）
        // 我们需要回退文件位置：由于 fscanf 在失败时可能会 partially consume, 使用下面的后备读取
        // 清理流直到行结束并尝试用 sscanf 从缓冲行解析
        char line[512];
        if (fgets(line, sizeof(line), fp) == NULL) break; // 没有更多行
        // 尝试用 sscanf 读取前4个字段（兼容旧数据）
        if (sscanf(line, "%9s %19s %19s %19s", tmp.plate, tmp.brand, tmp.model, tmp.owner) == 4) {
            tmp.violation = VIOLATION_NONE;
            tmp.purchase_date.year = 0; tmp.purchase_date.month = 0; tmp.purchase_date.day = 0;
            tmp.purchase_price = 0.0;
            ctx->cars[ctx->car_count++] = tmp;
            continue;
        }
        // 无法解析，跳过
        if (feof(fp)) break;
    }

    fclose(fp);
    return 1;
}
int save_policies(const AppContext* ctx)
{
    FILE* fp;
    int i;

    fp = fopen("policies.txt", "w");
    if (fp == NULL) {
        printf("保单数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->policy_count; i++) {
        fprintf(fp, "%s %s %s %d %.2f %.2f %d %d %d %d %d %d %s\n",
            ctx->policies[i].policy_id,
            ctx->policies[i].plate,
            ctx->policies[i].owner,
            ctx->policies[i].active,
            ctx->policies[i].coverage_limit,
            ctx->policies[i].premium,
            ctx->policies[i].start_date.year,
            ctx->policies[i].start_date.month,
            ctx->policies[i].start_date.day,
            ctx->policies[i].end_date.year,
            ctx->policies[i].end_date.month,
            ctx->policies[i].end_date.day,
            ctx->policies[i].coverage_desc);
    }

    fclose(fp);
    return 1;
}
int load_policies(AppContext* ctx)
{
    FILE* fp;

    fp = fopen("policies.txt", "r");
    if (fp == NULL) {
        ctx->policy_count = 0;
        return 0;
    }

    ctx->policy_count = 0;

    while (ctx->policy_count < MAX_POLICIES &&
        fscanf(fp, "%23s %9s %19s %d %lf %lf %d %d %d %d %d %d %63s",
            ctx->policies[ctx->policy_count].policy_id,
            ctx->policies[ctx->policy_count].plate,
            ctx->policies[ctx->policy_count].owner,
            &ctx->policies[ctx->policy_count].active,
            &ctx->policies[ctx->policy_count].coverage_limit,
            &ctx->policies[ctx->policy_count].premium,
            &ctx->policies[ctx->policy_count].start_date.year,
            &ctx->policies[ctx->policy_count].start_date.month,
            &ctx->policies[ctx->policy_count].start_date.day,
            &ctx->policies[ctx->policy_count].end_date.year,
            &ctx->policies[ctx->policy_count].end_date.month,
            &ctx->policies[ctx->policy_count].end_date.day,
            ctx->policies[ctx->policy_count].coverage_desc) == 13)
    {
        ctx->policy_count++;
    }

    fclose(fp);
    return 1;
}
int save_claims(const AppContext* ctx)
{
    FILE* fp;
    int i;

    fp = fopen("claims.txt", "w");
    if (fp == NULL) {
        printf("理赔数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->claim_count; i++) {
        fprintf(fp, "%s %s %s %s %.2f %.2f %d %d %d %d %s\n",
            ctx->claims[i].claim_id,
            ctx->claims[i].policy_id,
            ctx->claims[i].plate,
            ctx->claims[i].claimant,
            ctx->claims[i].request_amount,
            ctx->claims[i].approved_amount,
            ctx->claims[i].settled,
            ctx->claims[i].claim_date.year,
            ctx->claims[i].claim_date.month,
            ctx->claims[i].claim_date.day,
            ctx->claims[i].description);
    }

    fclose(fp);
    return 1;
}
int load_claims(AppContext* ctx)
{
    FILE* fp;

    fp = fopen("claims.txt", "r");
    if (fp == NULL) {
        ctx->claim_count = 0;
        return 0;
    }

    ctx->claim_count = 0;

    while (ctx->claim_count < MAX_CLAIMS &&
        fscanf(fp, "%23s %23s %9s %19s %lf %lf %d %d %d %d %255s",
            ctx->claims[ctx->claim_count].claim_id,
            ctx->claims[ctx->claim_count].policy_id,
            ctx->claims[ctx->claim_count].plate,
            ctx->claims[ctx->claim_count].claimant,
            &ctx->claims[ctx->claim_count].request_amount,
            &ctx->claims[ctx->claim_count].approved_amount,
            &ctx->claims[ctx->claim_count].settled,
            &ctx->claims[ctx->claim_count].claim_date.year,
            &ctx->claims[ctx->claim_count].claim_date.month,
            &ctx->claims[ctx->claim_count].claim_date.day,
            ctx->claims[ctx->claim_count].description) == 11)
    {
        ctx->claim_count++;
    }

    fclose(fp);
    return 1;
}