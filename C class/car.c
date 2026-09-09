#define _CRT_SECURE_NO_WARNINGS
#include "car.h"
#include "app_utils.h"
#include "insurance.h"
#include "user.h"

#include <string.h>

/*
 * car.c —— 车辆业务模块
 *
 * add_car/remove_car 等底层函数直接维护车辆数组；*_for_current_user 包装
 * 函数先检查登录身份和权限，再调用底层函数。GUI 应优先使用包装函数。
 * 所有会成功/失败的函数统一使用 1=成功、0=失败。
 */

/* ================= 基础初始化与 CRUD ================= */
/*
 * 初始化上下文。!ctx 等价于 ctx == NULL。memset 把整个结构体的每个字节
 * 清零，因此所有计数为 0、所有字符串也以 '\0' 开头而成为空字符串。
 */
void init_app(AppContext *ctx)
{
    if (!ctx)
        return;

    memset(ctx, 0, sizeof(AppContext));
    ctx->current_user[0] = '\0';
}

/*
 * 顺序扫描有效车辆并返回下标，找不到返回 -1。strcmp 相等时返回 0，
 * 因此字符串相等的判断写成 strcmp(a, b) == 0。
 */
static int find_car_index(const AppContext *ctx, const char *plate)
{
    if (!ctx || !plate)
        return -1;
    for (int i = 0; i < ctx->car_count; ++i)
    {
        if (strcmp(ctx->cars[i].plate, plate) == 0)
            return i;
    }
    return -1;
}

/*
 * 复用 find_car_index，并用 &ctx->cars[idx] 取得数组元素地址。返回的是
 * AppContext 内部对象，调用者不能对它调用 free。
 */
Car *find_car(AppContext *ctx, const char *plate)
{
    if (!ctx || !plate)
        return NULL;
    int idx = find_car_index(ctx, plate);
    if (idx == -1)
        return NULL;
    return &ctx->cars[idx];
}

/*
 * 添加车辆：成功返回 1；空参数、容量已满或车牌重复时返回 0。
 *
 * 先填充 cars[car_count] 这个空槽位，全部成功后才增加 car_count。
 */
int add_car(AppContext *ctx, const char *plate, const char *brand, const char *model,
            const char *owner, ViolationLevel violation, Date purchase_date, double purchase_price)
{
    if (!ctx || !validate_plate(plate) ||
        !validate_string_len(brand, (int)sizeof(ctx->cars[0].brand)) ||
        !validate_string_len(model, (int)sizeof(ctx->cars[0].model)) ||
        !validate_string_len(owner, (int)sizeof(ctx->cars[0].owner)) ||
        !date_is_valid(&purchase_date) || !validate_price_value(purchase_price))
        return 0;
    if (ctx->car_count >= MAX_CARS)
        return 0;
    if (find_car_index(ctx, plate) != -1)
        return 0;

    /* c 是待填充元素的地址；-> 用于通过结构体指针访问成员。 */
    Car *c = &ctx->cars[ctx->car_count];
    /* copy_text 统一检查容量并保证字符串以 '\0' 结束。 */
    copy_text(c->plate, sizeof(c->plate), plate);
    copy_text(c->brand, sizeof(c->brand), brand);
    copy_text(c->model, sizeof(c->model), model);
    copy_text(c->owner, sizeof(c->owner), owner);

    c->violation = violation;
    c->purchase_date = purchase_date;
    c->purchase_price = purchase_price;

    ctx->car_count++;
    return 1;
}

/*
 * 删除车辆：成功返回 1，失败返回 0。先清理关联保险数据，再把删除位置
 * 后面的结构体逐项前移，保证不会遗留孤立保单或理赔。
 */
int remove_car(AppContext *ctx, const char *plate)
{
    if (!ctx || !plate)
        return 0;
    int idx = find_car_index(ctx, plate);
    if (idx == -1)
        return 0;
    remove_insurance_for_plate(ctx, plate);
    /* C 结构体支持整体赋值，内部数组和所有字段都会一起复制。 */
    for (int i = idx; i < ctx->car_count - 1; ++i)
    {
        ctx->cars[i] = ctx->cars[i + 1];
    }
    ctx->car_count--;
    memset(&ctx->cars[ctx->car_count], 0, sizeof(ctx->cars[0]));
    return 1;
}

/*
 * 修改车辆信息。三个字符串参数可为 NULL，表示该字段保持不变；日期、
 * 违章和价格按值传入，会直接覆盖旧值。成功返回 1，失败返回 0。
 */
int modify_car(AppContext *ctx, const char *plate, const char *new_brand, const char *new_model,
               const char *new_owner, ViolationLevel new_violation, Date new_purchase_date,
               double new_purchase_price)
{
    if (!ctx || !plate || !new_brand || !new_model ||
        !validate_string_len(new_brand, (int)sizeof(ctx->cars[0].brand)) ||
        !validate_string_len(new_model, (int)sizeof(ctx->cars[0].model)) ||
        !date_is_valid(&new_purchase_date) || !validate_price_value(new_purchase_price))
        return 0;
    Car *c = find_car(ctx, plate);
    if (!c)
        return 0;
    if (new_brand)
    {
        copy_text(c->brand, sizeof(c->brand), new_brand);
    }
    if (new_model)
    {
        copy_text(c->model, sizeof(c->model), new_model);
    }
    if (new_owner)
    {
        if (!validate_string_len(new_owner, (int)sizeof(c->owner)))
            return 0;
        copy_text(c->owner, sizeof(c->owner), new_owner);
    }
    c->violation = new_violation;
    c->purchase_date = new_purchase_date;
    c->purchase_price = new_purchase_price;
    return 1;
}

/* ================= 当前用户可见性与权限控制 ================= */
/*
 * 收集可见车辆的数组下标，返回实际写入 indices 的数量。这里只复制 int
 * 下标而不复制 Car。普通用户会被强制为 only_mine=1，管理员可看全部。
 */
int collect_visible_car_indices(const AppContext *ctx, int only_mine, int *indices, int max_count)
{
    int i, count = 0;

    if (!ctx || !indices || max_count <= 0)
        return 0;

    if (!app_is_admin(ctx))
        only_mine = 1;

    for (i = 0; i < ctx->car_count && count < max_count; i++)
    {
        if (!only_mine || strcmp(ctx->cars[i].owner, ctx->current_user) == 0)
            /* 先写入 indices[count]，再由后缀 ++ 把 count 加 1。 */
            indices[count++] = i;
    }

    return count;
}
/*
 * 在权限范围内查车。普通用户查询他人车辆也得到 NULL，调用者无法区分
 * “不存在”和“没有权限”，从接口层阻止越权访问。
 */
const Car *find_visible_car(const AppContext *ctx, const char *plate)
{
    int i;

    if (!ctx || !plate)
        return NULL;

    for (i = 0; i < ctx->car_count; i++)
    {
        if (strcmp(ctx->cars[i].plate, plate) == 0)
        {
            if (app_is_admin(ctx) || strcmp(ctx->cars[i].owner, ctx->current_user) == 0)
                return &ctx->cars[i];
            return NULL;
        }
    }

    return NULL;
}

/*
 * 安全新增入口。管理员必须指定存在的车主；普通用户填写的 requested_owner
 * 会被忽略，车辆强制归当前用户。返回 1=成功、0=失败。
 */
int add_car_for_current_user(AppContext *ctx, const char *plate, const char *brand,
                             const char *model, const char *requested_owner,
                             ViolationLevel violation, Date purchase_date, double purchase_price)
{
    char owner[20];

    if (!ctx || !plate || !brand || !model)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    if (app_is_admin(ctx))
    {
        if (!requested_owner || !user_exists(ctx, requested_owner))
            return 0;
        copy_text(owner, sizeof(owner), requested_owner);
    }
    else
    {
        copy_text(owner, sizeof(owner), ctx->current_user);
    }

    return add_car(ctx, plate, brand, model, owner, violation, purchase_date, purchase_price);
}

/*
 * 安全修改入口。普通用户只能修改自己的车辆；向底层传 NULL 作为 new_owner，
 * 所以此接口不能改变车辆所有权。
 */
int modify_car_for_current_user(AppContext *ctx, const char *plate, const char *new_brand,
                                const char *new_model, ViolationLevel new_violation,
                                Date new_purchase_date, double new_purchase_price)
{
    Car *car;

    if (!ctx || !plate)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    if (!app_is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0)
        return 0;

    return modify_car(ctx, plate, new_brand, new_model, NULL, new_violation, new_purchase_date,
                      new_purchase_price);
}

/* 安全删除入口：完成登录、存在性和所有权检查后才调用底层删除。 */
int remove_car_for_current_user(AppContext *ctx, const char *plate)
{
    Car *car;

    if (!ctx || !plate)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    if (!app_is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0)
        return 0;

    return remove_car(ctx, plate);
}
