//#define _CRT_SECURE_NO_WARNINGS
//#include "common.h"
//
//int register_user(AppContext* ctx);
//int login_user(AppContext* ctx);
//
//void car_menu(AppContext* ctx) {
//    int choice;
//    char plate[10];
//    char brand[20];
//    char model[20];
//
//    while (1) {
//        system("cls");
//
//        printf("====== 车辆管理 ======\n");
//        printf("当前用户：%s\n", ctx->current_user);
//        printf("1. 添加车辆\n");
//        printf("2. 查看车辆\n");
//        printf("3. 删除车辆\n");
//        printf("0. 返回主菜单\n");
//        printf("请选择: ");
//
//        if (scanf("%d", &choice) != 1) {
//            printf("输入错误！\n");
//            while (getchar() != '\n');
//            continue;
//        }
//        getchar();
//
//        switch (choice) {
//        case 1:
//            printf("请输入车牌号: ");
//            scanf("%9s", plate);
//
//            printf("请输入品牌: ");
//            scanf("%19s", brand);
//
//            printf("请输入型号: ");
//            scanf("%19s", model);
//
//            if (add_car(ctx, plate, brand, model, ctx->current_user) == 0) {
//                printf("车辆添加成功！\n");
//            }
//            else {
//                printf("车辆添加失败，可能车牌已存在或车辆数量已满。\n");
//            }
//            break;
//
//        case 2:
//            list_cars(ctx);
//            break;
//
//        case 3:
//            printf("请输入要删除的车牌号: ");
//            scanf("%9s", plate);
//
//            if (remove_car(ctx, plate) == 0) {
//                printf("车辆删除成功！\n");
//            }
//            else {
//                printf("车辆删除失败，未找到该车辆。\n");
//            }
//            break;
//
//        case 0:
//            return;
//
//        default:
//            printf("无效选择。\n");
//        }
//
//        printf("\n按回车键继续...");
//        getchar();
//        getchar();
//    }
//}
//
//int main() {
//    int choice;
//    AppContext ctx;
//
//    init_app(&ctx);
//
//    while (1) {
//        system("cls");
//
//        printf("====== 好车主助手 ======\n"); 
//
//        if (ctx.current_user[0] != '\0') {
//            printf("当前登录用户：%s\n", ctx.current_user);
//        }
//        else {
//            printf("当前状态：未登录\n");
//        }
//
//        printf("1. 用户注册\n");
//        printf("2. 用户登录\n");
//        printf("3. 注销登录\n");
//        printf("0. 退出\n");
//        printf("请选择: ");
//
//        if (scanf("%d", &choice) != 1) {
//            printf("输入错误！\n");
//            while (getchar() != '\n');
//            continue;
//        }
//        getchar();
//
//        switch (choice) {
//        case 1:
//            register_user(&ctx);
//            break;
//
//        case 2:
//            if (login_user(&ctx)) {
//                car_menu(&ctx);
//            }
//            break;
//
//        case 3:
//            ctx.current_user[0] = '\0';
//            printf("已注销登录。\n");
//            break;
//
//        case 0:
//            printf("退出系统，再见！\n");
//            return 0;
//
//        default:
//            printf("无效选择。\n");
//        }
//
//        printf("\n按回车键继续...");
//        getchar();
//    }
//}

#define _CRT_SECURE_NO_WARNINGS
#include "common.h"

int register_user(AppContext* ctx);
int login_user(AppContext* ctx);

static void flush_stdin(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

static ViolationLevel prompt_violation(void) {
    int v;
    printf("请选择违章等级：0.无 1.较轻微 2.轻微 3.中等 4.较严重 5.严重\n");
    printf("输入编号：");
    if (scanf("%d", &v) != 1) { flush_stdin(); return VIOLATION_NONE; }
    flush_stdin();
    if (v < VIOLATION_NONE || v > VIOLATION_SERIOUS) return VIOLATION_NONE;
    return (ViolationLevel)v;
}

static Date prompt_date(void) {
    Date d = {0,0,0};
    printf("请输入购买日期（格式 YYYY MM DD），若不确定可输入 0 0 0：");
    if (scanf("%d %d %d", &d.year, &d.month, &d.day) != 3) {
        flush_stdin();
        d.year = d.month = d.day = 0;
    } else {
        flush_stdin();
    }
    return d;
}

static double prompt_price(void) {
    double p = 0.0;
    printf("请输入购入价格（元），若不确定输入 0：");
    if (scanf("%lf", &p) != 1) { flush_stdin(); return 0.0; }
    flush_stdin();
    return p;
}

void car_menu(AppContext* ctx) {
    int choice;
    char plate[16];
    char brand[64];
    char model[64];

    while (1) {
        printf("\n===== 车辆管理菜单 =====\n");
        printf("当前用户：%s\n", ctx->current_user[0] ? ctx->current_user : "未登录");
        printf("1. 添加车辆\n");
        printf("2. 列出所有车辆\n");
        printf("3. 列出我的车辆\n");
        printf("4. 查找车辆（按车牌）\n");
        printf("5. 修改车辆信息\n");
        printf("6. 删除车辆\n");
        printf("0. 返回上一级\n");
        printf("请选择：");

        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请输入数字！\n");
            flush_stdin();
            continue;
        }
        flush_stdin();

        switch (choice) {
        case 1: {
            if (ctx->current_user[0] == '\0') {
                printf("请先登录再添加车辆。\n");
                break;
            }
            printf("请输入车牌号：");
            if (scanf("%15s", plate) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();

            printf("请输入品牌：");
            if (scanf("%63s", brand) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();

            printf("请输入型号：");
            if (scanf("%63s", model) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();

            ViolationLevel v = prompt_violation();
            Date date = prompt_date();
            double price = prompt_price();

            if (add_car(ctx, plate, brand, model, ctx->current_user, v, date, price) == 0) {
                if (!save_cars(ctx)) printf("警告：保存车辆文件失败。\n");
                printf("添加车辆成功！\n");
            } else {
                printf("添加车辆失败（可能车牌已存在或达到上限）。\n");
            }
            break;
        }

        case 2:
            list_cars(ctx);
            break;

        case 3: {
            int found = 0;
            if (ctx->current_user[0] == '\0') { printf("未登录。\n"); break; }
            printf("===== 我的车辆 =====\n");
            for (int i = 0; i < ctx->car_count; ++i) {
                if (strcmp(ctx->cars[i].owner, ctx->current_user) == 0) {
                    const Car* c = &ctx->cars[i];
                    printf("%s  %s  %s\n", c->plate, c->brand, c->model);
                    found = 1;
                }
            }
            if (!found) printf("未找到属于您的车辆。\n");
            break;
        }

        case 4: {
            printf("请输入要查询的车牌号：");
            if (scanf("%15s", plate) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();
            Car* f = find_car(ctx, plate);
            if (!f) {
                printf("未找到该车辆：%s\n", plate);
            } else {
                printf("车牌：%s\n品牌：%s\n型号：%s\n车主：%s\n违章：%d\n购买日期：%04d-%02d-%02d\n购入价：%.2f\n",
                       f->plate, f->brand, f->model, f->owner,
                       (int)f->violation,
                       f->purchase_date.year, f->purchase_date.month, f->purchase_date.day,
                       f->purchase_price);
            }
            break;
        }

        case 5: {
            printf("请输入要修改的车牌号：");
            if (scanf("%15s", plate) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();
            Car* f = find_car(ctx, plate);
            if (!f) { printf("未找到该车辆。\n"); break; }
            if (strncmp(f->owner, ctx->current_user, sizeof(f->owner)) != 0) {
                printf("只能修改属于当前登录用户的车辆（车主：%s）。\n", f->owner);
                break;
            }
            printf("输入 '-' 表示保持不变。\n");
            char new_brand[64] = {0}, new_model[64] = {0};
            printf("新的品牌（或输入 - 保持不变）：");
            if (scanf("%63s", new_brand) == 1) flush_stdin(); else flush_stdin();
            printf("新的型号（或输入 - 保持不变）：");
            if (scanf("%63s", new_model) == 1) flush_stdin(); else flush_stdin();

            ViolationLevel v = prompt_violation();
            Date date = prompt_date();
            double price = prompt_price();

            const char* nb = (new_brand[0] == '-' ? NULL : new_brand);
            const char* nm = (new_model[0] == '-' ? NULL : new_model);

            if (modify_car(ctx, plate, nb, nm, NULL, v, date, price) == 0) {
                if (!save_cars(ctx)) printf("警告：保存车辆文件失败。\n");
                printf("修改成功。\n");
            } else {
                printf("修改失败。\n");
            }
            break;
        }

        case 6: {
            printf("请输入要删除的车牌号：");
            if (scanf("%15s", plate) != 1) { printf("输入错误。\n"); flush_stdin(); break; }
            flush_stdin();
            Car* f = find_car(ctx, plate);
            if (!f) { printf("未找到该车辆。\n"); break; }
            if (strncmp(f->owner, ctx->current_user, sizeof(f->owner)) != 0) {
                printf("只能删除属于当前登录用户的车辆（车主：%s）。\n", f->owner);
                break;
            }
            if (remove_car(ctx, plate) == 0) {
                if (!save_cars(ctx)) printf("警告：保存车辆文件失败。\n");
                printf("删除成功！\n");
            } else {
                printf("删除失败！\n");
            }
            break;
        }

        case 0:
            return;

        default:
            printf("无效选择！\n");
        }
    }
}

void user_menu(AppContext* ctx) {
    int choice;

    while (1) {
        printf("\n===== 用户菜单 =====\n");
        printf("1. 注册\n");
        printf("2. 登录\n");
        printf("3. 注销登录\n");
        printf("4. 车辆管理\n");
        printf("0. 返回主菜单\n");
        printf("请选择：");

        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请输入数字！\n");
            flush_stdin();
            continue;
        }
        flush_stdin();

        switch (choice) {
        case 1:
            register_user(ctx);
            break;

        case 2:
            if (login_user(ctx)) {
                printf("登录成功，进入车辆管理。\n");
                car_menu(ctx);
            }
            break;

        case 3:
            ctx->current_user[0] = '\0';
            printf("已注销登录。\n");
            break;

        case 4:
            car_menu(ctx);
            break;

        case 0:
            printf("退出系统，再见！\n");
            return;

        default:
            printf("无效选择！\n");
        }
    }
}

int main() {
    AppContext ctx;

    init_app(&ctx);
    load_users(&ctx);
    load_cars(&ctx);

    user_menu(&ctx);

    return 0;
}