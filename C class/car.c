#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

void init_app(AppContext* ctx) {
    if (!ctx) return;
    ctx->user_count = 0;
    ctx->car_count = 0;
    ctx->current_user[0] = '\0';
}

int find_car_index(const AppContext* ctx, const char* plate) {
    if (!ctx || !plate) return -1;
    for (int i = 0; i < ctx->car_count; ++i) {
        if (strcmp(ctx->cars[i].plate, plate) == 0) return i;
    }
    return -1;
}

Car* find_car(AppContext* ctx, const char* plate) {
    if (!ctx || !plate) return NULL;
    int idx = find_car_index(ctx, plate);
    if (idx == -1) return NULL;
    return &ctx->cars[idx];
}

int add_car(AppContext* ctx, const char* plate, const char* brand, const char* model, const char* owner) {
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

    ctx->car_count++;
    return 0;
}

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
    ctx->car_count--;
    return 0;
}

int modify_car(AppContext* ctx, const char* plate, const char* new_brand, const char* new_model, const char* new_owner) {
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
    return 0;
}

void list_cars(const AppContext* ctx) {
    if (!ctx) return;
    if (ctx->car_count == 0) {
        printf("没有车辆记录。\n");
        return;
    }
    printf("序号  车牌号    品牌           型号           车主\n");
    printf("--------------------------------------------------\n");
    for (int i = 0; i < ctx->car_count; ++i) {
        const Car* c = &ctx->cars[i];
        printf("%-4d  %-8s  %-12s  %-12s  %-8s\n", i + 1, c->plate, c->brand, c->model, c->owner);
    }
}
