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

void car_menu(AppContext* ctx) {
    int choice;
    char plate[10];
    char brand[20];
    char model[20];

    while (1) {
        printf("\n===== 车辆管理菜单 =====\n");
        printf("当前用户：%s\n", ctx->current_user);
        printf("1. 添加车辆\n");
        printf("2. 查看车辆\n");
        printf("3. 删除车辆\n");
        printf("0. 返回上一级\n");
        printf("请选择：");

        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请输入数字！\n");
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
        case 1:
            printf("请输入车牌号：");
            scanf("%9s", plate);

            printf("请输入品牌：");
            scanf("%19s", brand);

            printf("请输入型号：");
            scanf("%19s", model);

            if (add_car(ctx, plate, brand, model, ctx->current_user) == 0) {
                printf("添加车辆成功！\n");
            }
            else {
                printf("添加车辆失败！\n");
            }
            break;

        case 2:
            list_cars(ctx);
            break;

        case 3:
            printf("请输入要删除的车牌号：");
            scanf("%9s", plate);

            if (remove_car(ctx, plate) == 0) {
                printf("删除成功！\n");
            }
            else {
                printf("删除失败！\n");
            }
            break;

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
        printf("0. 返回主菜单\n");
        printf("请选择：");

        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请输入数字！\n");
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
        case 1:
            register_user(ctx);
            break;

        case 2:
            if (login_user(ctx)) {
                car_menu(ctx);
            }
            break;

        case 3:
            ctx->current_user[0] = '\0';
            printf("已注销登录。\n");
            break;

        case 0:
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
    user_menu(&ctx);

    return 0;
}