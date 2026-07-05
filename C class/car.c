#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

/* ================= 辅助函数 ================= */
/* 违章等级转字符串，用于控制台显示 */
static const char* violation_to_string(ViolationLevel v) {
    switch (v) {
    case VIOLATION_VERY_MINOR: return "较轻微";
    case VIOLATION_MINOR: return "轻微";
    case VIOLATION_MODERATE: return "中等";
    case VIOLATION_RELATIVELY_SERIOUS: return "较严重";
    case VIOLATION_SERIOUS: return "严重";
    default: return "无";
    }
}
/* ================= 基础初始化与 CRUD ================= */
/* 初始化应用上下文 */
void init_app(AppContext* ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(AppContext));
    ctx->current_user[0] = '\0';
    }

/* 按车牌查找车辆下标，找不到返回 -1 */
int find_car_index(const AppContext* ctx, const char* plate) {
    if (!ctx || !plate) return -1;
    for (int i = 0; i < ctx->car_count; ++i) {
        if (strcmp(ctx->cars[i].plate, plate) == 0) return i;
    }
    return -1;
}

/* 按车牌查找车辆指针，找不到返回 NULL */
Car* find_car(AppContext* ctx, const char* plate) {
    if (!ctx || !plate) return NULL;
    int idx = find_car_index(ctx, plate);
    if (idx == -1) return NULL;
    return &ctx->cars[idx];
}


/* 添加车辆：成功返回 0，失败返回 -1 */
int add_car(AppContext* ctx, const char* plate, const char* brand, const char* model, const char* owner,
            ViolationLevel violation, Date purchase_date, double purchase_price) {
    if (!ctx || !plate || !brand || !model || !owner) return -1;
    if (ctx->car_count >= MAX_CARS) return -1;
    if (find_car_index(ctx, plate) != -1) return -1; // 已存在

    Car* c = &ctx->cars[ctx->car_count];
    strncpy(c->plate, plate, sizeof(c->plate) - 1);
    c->plate[sizeof(c->plate) - 1] = '\0';
    strncpy(c->brand, brand, sizeof(c->brand) - 1);
    c->brand[sizeof(c->brand) - 1] = '\0';
    strncpy(c->model, model, sizeof(c->model) - 1);
    c->model[sizeof(c->model) - 1] = '\0';
    strncpy(c->owner, owner, sizeof(c->owner) - 1);
    c->owner[sizeof(c->owner) - 1] = '\0';

    c->violation = violation;
    c->purchase_date = purchase_date;
    c->purchase_price = purchase_price;

    ctx->car_count++;
    return 0;
}

/* 删除车辆：成功返回 0，失败返回 -1 */
int remove_car(AppContext* ctx, const char* plate) {
    if (!ctx || !plate) return -1;
    int idx = find_car_index(ctx, plate);
    if (idx == -1) return -1;
    for (int i = idx; i < ctx->car_count - 1; ++i) {
        ctx->cars[i] = ctx->cars[i + 1];
    }
    // 清空最后一条
    ctx->cars[ctx->car_count - 1].plate[0] = '\0';
    ctx->cars[ctx->car_count - 1].brand[0] = '\0';
    ctx->cars[ctx->car_count - 1].model[0] = '\0';
    ctx->cars[ctx->car_count - 1].owner[0] = '\0';
    ctx->cars[ctx->car_count - 1].violation = VIOLATION_NONE;
    ctx->cars[ctx->car_count - 1].purchase_date.year = 0;
    ctx->cars[ctx->car_count - 1].purchase_date.month = 0;
    ctx->cars[ctx->car_count - 1].purchase_date.day = 0;
    ctx->cars[ctx->car_count - 1].purchase_price = 0.0;
    ctx->car_count--;
    return 0;
}



/* 修改车辆信息：成功返回 0，失败返回 -1 */
int modify_car(AppContext* ctx, const char* plate, const char* new_brand, const char* new_model, const char* new_owner,
               ViolationLevel new_violation, Date new_purchase_date, double new_purchase_price) {
    if (!ctx || !plate) return -1;
    Car* c = find_car(ctx, plate);
    if (!c) return -1;
    if (new_brand) {
        strncpy(c->brand, new_brand, sizeof(c->brand) - 1);
        c->brand[sizeof(c->brand) - 1] = '\0';
    }
    if (new_model) {
        strncpy(c->model, new_model, sizeof(c->model) - 1);
        c->model[sizeof(c->model) - 1] = '\0';
    }
    if (new_owner) {
        strncpy(c->owner, new_owner, sizeof(c->owner) - 1);
        c->owner[sizeof(c->owner) - 1] = '\0';
    }
    c->violation = new_violation;
    c->purchase_date = new_purchase_date;
    c->purchase_price = new_purchase_price;
    return 0;
}

/* ================= 控制台列表输出 ================= */
/* 列出全部车辆信息 */
void list_cars(const AppContext* ctx) {
    if (!ctx) return;
    if (ctx->car_count == 0) {
        printf("没有车辆记录。\n");
        return;
    }
    printf("序号  车牌号    品牌           型号           车主       违章        购买日期     购入价(元)\n");
    printf("------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < ctx->car_count; ++i) {
        const Car* c = &ctx->cars[i];
        printf("%-4d  %-8s  %-12s  %-12s  %-8s  %-10s  %04d-%02d-%02d  %.2f\n",
               i + 1, c->plate, c->brand, c->model, c->owner,
               violation_to_string(c->violation),
               c->purchase_date.year, c->purchase_date.month, c->purchase_date.day,
               c->purchase_price);
    }
}

/* ================= 当前用户可见性与权限控制 ================= */
/* 收集当前用户可见的车辆下标 */
int collect_visible_car_indices(const AppContext* ctx, int only_mine, int* indices, int max_count)
{
    int i, count = 0;

    if (!ctx || !indices || max_count <= 0)
        return 0;

    if (!app_is_admin(ctx))
        only_mine = 1;

    for (i = 0; i < ctx->car_count && count < max_count; i++) {
        if (!only_mine || strcmp(ctx->cars[i].owner, ctx->current_user) == 0)
            indices[count++] = i;
    }

    return count;
}
/* 在当前用户可见范围内查找车辆 */
const Car* find_visible_car(const AppContext* ctx, const char* plate)
{
    int i;

    if (!ctx || !plate)
        return NULL;

    for (i = 0; i < ctx->car_count; i++) {
        if (strcmp(ctx->cars[i].plate, plate) == 0) {
            if (app_is_admin(ctx) || strcmp(ctx->cars[i].owner, ctx->current_user) == 0)
                return &ctx->cars[i];
            return NULL;
        }
    }

    return NULL;
}

/* 以当前用户身份添加车辆 */
int add_car_for_current_user(AppContext* ctx, const char* plate, const char* brand,
    const char* model, const char* requested_owner,
    ViolationLevel violation, Date purchase_date, double purchase_price)
{
    char owner[20];

    if (!ctx || !plate || !brand || !model)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    if (app_is_admin(ctx)) {
        if (!requested_owner || !user_exists(ctx, requested_owner))
            return 0;
        strcpy(owner, requested_owner);
    }
    else {
        strcpy(owner, ctx->current_user);
    }

    return add_car(ctx, plate, brand, model, owner, violation, purchase_date, purchase_price) == 0;
}

/* 以当前用户身份修改车辆 */
int modify_car_for_current_user(AppContext* ctx, const char* plate,
    const char* new_brand, const char* new_model,
    ViolationLevel new_violation, Date new_purchase_date, double new_purchase_price)
{
    Car* car;

    if (!ctx || !plate)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    if (!app_is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0)
        return 0;

    return modify_car(ctx, plate, new_brand, new_model, NULL,
        new_violation, new_purchase_date, new_purchase_price) == 0;
}

/* 以当前用户身份删除车辆 */
int remove_car_for_current_user(AppContext* ctx, const char* plate)
{
    Car* car;

    if (!ctx || !plate)
        return 0;

    if (!app_is_logged_in(ctx))
        return 0;

    car = find_car(ctx, plate);
    if (!car)
        return 0;

    if (!app_is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0)
        return 0;

    return remove_car(ctx, plate) == 0;
}
