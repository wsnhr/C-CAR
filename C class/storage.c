#define _CRT_SECURE_NO_WARNINGS
#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * storage.c —— 文本文件持久化和操作日志
 *
 * AppContext 只存在于进程内存中，程序退出后会消失。本文件用 fprintf 把
 * 结构体字段写成文本，用 fscanf/fgets 在下次启动时读回。文件名是相对
 * 路径，因此实际读写目录取决于程序启动时的“工作目录”。
 *
 * FILE* 是标准库的文件流指针；fopen 成功返回有效指针，失败返回 NULL；
 * 每个成功打开的文件都必须用 fclose 关闭。
 */

/* ================= 用户数据持久化 ================= */
/*
 * 全量重写 users.txt，每行是用户名、密码盐/哈希、实名盐/哈希。
 * 文件中不保存明文密码、姓名或身份证号。
 */
int save_users(const AppContext *ctx)
{
    FILE *fp;
    int i;

    if (!ctx)
        return 0;

    fp = fopen("users.txt", "w");

    if (fp == NULL)
    {
        printf("用户数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->user_count; ++i)
        fprintf(fp, "%s %s %s %s %s\n", ctx->users[i].username, ctx->users[i].salt,
                ctx->users[i].password, ctx->users[i].identity_salt,
                ctx->users[i].identity_hash);

    fclose(fp);
    return 1;
}
/*
 * 按行加载用户；实名版本固定使用五个字段，格式不完整的记录会被忽略。
 */
int load_users(AppContext *ctx)
{
    FILE *fp;
    char line[256];

    if (!ctx)
        return 0;

    fp = fopen("users.txt", "r");

    if (fp == NULL)
    {
        ctx->user_count = 0;
        return 0;
    }

    ctx->user_count = 0;

    while (ctx->user_count < MAX_USERS && fgets(line, sizeof(line), fp))
    {
        User *user = &ctx->users[ctx->user_count];
        int field_count;
        memset(user, 0, sizeof(*user));
        field_count = sscanf(line, "%19s %32s %64s %32s %64s", user->username, user->salt,
                             user->password, user->identity_salt, user->identity_hash);
        if (field_count == 5)
            ctx->user_count++;
        else
            memset(user, 0, sizeof(*user));
    }

    fclose(fp);
    return 1;
}
/* ================= 车辆数据持久化 ================= */
/*
 * 用 "w" 重建 cars.txt，再按数组顺序写出所有有效车辆。车辆类型追加在旧格式
 * 的末尾，因此旧版读取逻辑和已有数据的前九个字段仍保持原来的含义。
 */
int save_cars(const AppContext *ctx)
{
    FILE *fp;
    int i;

    if (!ctx)
        return 0;

    fp = fopen("cars.txt", "w"); // 重写整个车辆文件

    if (fp == NULL)
    {
        printf("车辆数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->car_count; i++)
    {
        fprintf(fp, "%s %s %s %s %d %d %d %d %.2f %d\n", ctx->cars[i].plate, ctx->cars[i].brand,
                ctx->cars[i].model, ctx->cars[i].owner, (int)ctx->cars[i].violation,
                ctx->cars[i].purchase_date.year, ctx->cars[i].purchase_date.month,
                ctx->cars[i].purchase_date.day, ctx->cars[i].purchase_price,
                (int)ctx->cars[i].vehicle_type);
    }

    fclose(fp);
    return 1;
}
/*
 * 加载车辆，同时兼容三种格式：带车型的 10 字段格式、未带车型的 9 字段
 * 格式和只有 4 个字符串的早期格式。旧记录统一按“小型车”处理。
 * 先用 fgets 读取完整一行，再用 sscanf 解析。这样新格式匹配失败时不会破坏
 * 文件位置，仍可用同一行尝试旧格式。
 *
 * 整数和浮点数目标前写 & 传入地址；字符数组名本身会转换为首元素地址。
 * %9s 等宽度限制确保输入最多写入“数组容量减一”个字符，为 '\0' 留出空间。
 */
int load_cars(AppContext *ctx)
{
    FILE *fp;
    char line[512];

    if (!ctx)
        return 0;

    fp = fopen("cars.txt", "r");
    if (fp == NULL)
    {
        ctx->car_count = 0;
        return 0;
    }

    ctx->car_count = 0;

    while (ctx->car_count < MAX_CARS && fgets(line, sizeof(line), fp) != NULL)
    {
        /* 先读入临时结构体，确认一整条记录有效后再复制进 ctx 数组。 */
        Car tmp = {0};
        int v, vehicle_type;
        int y, m, d;
        double price;
        int read_count;

        /* 新字段放在行尾，使旧版九字段记录不会发生字段错位。 */
        read_count = sscanf(line, "%9s %19s %19s %19s %d %d %d %d %lf %d", tmp.plate,
                            tmp.brand, tmp.model, tmp.owner, &v, &y, &m, &d, &price,
                            &vehicle_type);
        if (read_count >= 9)
        {
            tmp.vehicle_type = read_count == 10 &&
                                       vehicle_type >= VEHICLE_TYPE_MOTORCYCLE &&
                                       vehicle_type <= VEHICLE_TYPE_LARGE_CAR
                                   ? (VehicleType)vehicle_type
                                   : VEHICLE_TYPE_SMALL_CAR;
            tmp.violation = (v >= VIOLATION_NONE && v <= VIOLATION_SERIOUS) ? (ViolationLevel)v
                                                                            : VIOLATION_NONE;
            tmp.purchase_date.year = y;
            tmp.purchase_date.month = m;
            tmp.purchase_date.day = d;
            tmp.purchase_price = price;
            /* 结构体整体赋值；先使用当前下标，随后 car_count 自增。 */
            ctx->cars[ctx->car_count++] = tmp;
        }
        else if (read_count >= 4)
        {
            /* 最早格式没有车型、违章、日期和价格；车型仍按小型车兼容。 */
            tmp.vehicle_type = VEHICLE_TYPE_SMALL_CAR;
            tmp.violation = VIOLATION_NONE;
            ctx->cars[ctx->car_count++] = tmp;
        }
        /* 少于 4 个字段的损坏行不进入数组，下一轮继续读取下一行。 */
    }

    fclose(fp);
    return 1;
}
/* ================= 保单数据持久化 ================= */
/*
 * 全量重写保单。格式字符串中的占位符必须与后面的字段数量、类型和顺序
 * 一一对应，否则下次 load_policies 无法还原相同数据。
 */
int save_policies(const AppContext *ctx)
{
    FILE *fp;
    int i;

    if (!ctx)
        return 0;

    fp = fopen("policies.txt", "w");
    if (fp == NULL)
    {
        printf("保单数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->policy_count; i++)
    {
        fprintf(fp, "%s %s %s %d %.2f %.2f %d %d %d %d %d %d %s\n", ctx->policies[i].policy_id,
                ctx->policies[i].plate, ctx->policies[i].owner, ctx->policies[i].active,
                ctx->policies[i].coverage_limit, ctx->policies[i].premium,
                ctx->policies[i].start_date.year, ctx->policies[i].start_date.month,
                ctx->policies[i].start_date.day, ctx->policies[i].end_date.year,
                ctx->policies[i].end_date.month, ctx->policies[i].end_date.day,
                ctx->policies[i].coverage_desc);
    }

    fclose(fp);
    return 1;
}
/*
 * 每行必须成功转换 13 个字段才计为一条有效保单。对 int/double 成员传
 * 地址，对字符数组直接传数组名；循环结束后关闭文件并返回 1。
 */
int load_policies(AppContext *ctx)
{
    FILE *fp;

    if (!ctx)
        return 0;

    fp = fopen("policies.txt", "r");
    if (fp == NULL)
    {
        ctx->policy_count = 0;
        return 0;
    }

    ctx->policy_count = 0;

    while (ctx->policy_count < MAX_POLICIES &&
           fscanf(fp, "%23s %9s %19s %d %lf %lf %d %d %d %d %d %d %63s",
                  ctx->policies[ctx->policy_count].policy_id,
                  ctx->policies[ctx->policy_count].plate, ctx->policies[ctx->policy_count].owner,
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
/* ================= 理赔数据持久化 ================= */
/* 全量重写理赔文件；description 作为最后一个字符串字段保存。 */
int save_claims(const AppContext *ctx)
{
    FILE *fp;
    int i;

    if (!ctx)
        return 0;

    fp = fopen("claims.txt", "w");
    if (fp == NULL)
    {
        printf("理赔数据保存失败！\n");
        return 0;
    }

    for (i = 0; i < ctx->claim_count; i++)
    {
        fprintf(fp, "%s %s %s %s %.2f %.2f %d %d %d %d %s\n", ctx->claims[i].claim_id,
                ctx->claims[i].policy_id, ctx->claims[i].plate, ctx->claims[i].claimant,
                ctx->claims[i].request_amount, ctx->claims[i].approved_amount,
                ctx->claims[i].status, ctx->claims[i].claim_date.year,
                ctx->claims[i].claim_date.month, ctx->claims[i].claim_date.day,
                ctx->claims[i].description);
    }

    fclose(fp);
    return 1;
}
/* 加载理赔；只有 fscanf 返回 11 时才增加 claim_count。 */
int load_claims(AppContext *ctx)
{
    FILE *fp;

    if (!ctx)
        return 0;

    fp = fopen("claims.txt", "r");
    if (fp == NULL)
    {
        ctx->claim_count = 0;
        return 0;
    }

    ctx->claim_count = 0;

    while (ctx->claim_count < MAX_CLAIMS &&
           fscanf(fp, "%23s %23s %9s %19s %lf %lf %d %d %d %d %255s",
                  ctx->claims[ctx->claim_count].claim_id, ctx->claims[ctx->claim_count].policy_id,
                  ctx->claims[ctx->claim_count].plate, ctx->claims[ctx->claim_count].claimant,
                  &ctx->claims[ctx->claim_count].request_amount,
                  &ctx->claims[ctx->claim_count].approved_amount,
                  &ctx->claims[ctx->claim_count].status,
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
/* ================= 操作日志 ================= */
/*
 * 以追加模式记录时间、当前用户、动作、目标和详情。action ? action : ""
 * 是条件运算符：指针非 NULL 时用原字符串，否则用空字符串，避免把 NULL
 * 交给 fprintf 的 %s。strftime 负责格式化时间。
 */
void append_operation_log(const AppContext *ctx, const char *action, const char *target,
                          const char *details)
{
    FILE *fp = fopen("operations.log", "a");
    if (!fp)
        return;
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tmv);
    const char *user = ctx && ctx->current_user[0] ? ctx->current_user : "(none)";
    fprintf(fp, "%s | user=%s | action=%s | target=%s | details=%s\n", timebuf, user,
            action ? action : "", target ? target : "", details ? details : "");
    fclose(fp);
}
