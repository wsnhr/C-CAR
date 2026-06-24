//#define _CRT_SECURE_NO_WARNINGS
//#include "common.h"
//
//// EasyX GUI for C-CAR
//// 提示：需要在工程中安装并配置 EasyX（graphics.h、easyx.lib）才能编译。
//// 提供简单的图形界面：主菜单 + 列表车辆显示（只做演示，更多交互可扩展）
//
//#include <graphics.h>
//#include <windows.h>
//#include <stdio.h>
//
//// 按钮结构
//typedef struct {
//    int x, y, w, h;
//    const char* label;
//} Button;
//
//static void draw_button(const Button* b) {
//    setlinecolor(RGB(50, 50, 50));
//    setfillcolor(RGB(200, 220, 255));
//    bar(b->x, b->y, b->x + b->w, b->y + b->h);
//    setlinecolor(BLACK);
//    rectangle(b->x, b->y, b->x + b->w, b->y + b->h);
//    settextstyle(20, 0, _T("微软雅黑"));
//    settextcolor(BLACK);
//    int tx = b->x + 10;
//    int ty = b->y + (b->h - 20) / 2;
//    outtextxy(tx, ty, b->label);
//}
//
//static int pt_in_button(int px, int py, const Button* b) {
//    return px >= b->x && px <= b->x + b->w && py >= b->y && py <= b->y + b->h;
//}
//
//// 在窗口中列出车辆信息（简单分页）
//static void gui_list_cars(const AppContext* ctx) {
//    cleardevice();
//    setbkcolor(WHITE);
//    settextstyle(20, 0, _T("微软雅黑"));
//    settextcolor(BLACK);
//    outtextxy(20, 10, _T("车辆列表（图形界面展示）"));
//
//    int startY = 50;
//    int lineH = 24;
//    char buf[256];
//
//    if (!ctx || ctx->car_count == 0) {
//        outtextxy(20, startY, _T("无车辆记录。"));
//        outtextxy(20, startY + 40, _T("按鼠标或关闭窗口返回。"));
//        // 等待单击或关闭
//        MOUSEMSG msg;
//        while (1) {
//            if (MouseHit()) {
//                msg = GetMouseMsg();
//                if (msg.uMsg == WM_LBUTTONDOWN) break;
//            }
//            Sleep(10);
//        }
//        return;
//    }
//
//    for (int i = 0; i < ctx->car_count; ++i) {
//        const Car* c = &ctx->cars[i];
//        snprintf(buf, sizeof(buf), "%d. %s  %s  %s  违章:%d  购入:%04d-%02d-%02d  %.2f元",
//            i + 1, c->plate, c->brand, c->model, (int)c->violation,
//            c->purchase_date.year, c->purchase_date.month, c->purchase_date.day,
//            c->purchase_price);
//        outtextxy(20, startY + i * lineH, buf);
//        // 单页显示 20 条
//        if ((i + 1) % 20 == 0 && i + 1 < ctx->car_count) {
//            outtextxy(20, startY + 21 * lineH, _T("点击继续..."));
//            MOUSEMSG msg;
//            while (1) {
//                if (MouseHit()) {
//                    msg = GetMouseMsg();
//                    if (msg.uMsg == WM_LBUTTONDOWN) break;
//                }
//                Sleep(10);
//            }
//            cleardevice();
//            outtextxy(20, 10, _T("车辆列表（图形界面展示）"));
//        }
//    }
//    outtextxy(20, startY + ((ctx->car_count % 20) + 2) * lineH, _T("点击任意位置返回"));
//    MOUSEMSG msg;
//    while (1) {
//        if (MouseHit()) {
//            msg = GetMouseMsg();
//            if (msg.uMsg == WM_LBUTTONDOWN) break;
//        }
//        Sleep(10);
//    }
//}
//
//// 启动图形界面入口（在 main 中可调用 start_gui(&ctx) 进入 GUI）
//void start_gui(AppContext* ctx) {
//    // 初始化 EasyX 图形窗口
//    // 默认窗口大小 800x600
//    initgraph(800, 600);
//    setbkcolor(WHITE);
//    cleardevice();
//
//    Button btns[4] = {
//        {50, 100, 220, 50, "车辆管理"},
//        {50, 180, 220, 50, "保险管理"},
//        {50, 260, 220, 50, "用户管理"},
//        {50, 340, 220, 50, "退出程序"}
//    };
//
//    // 信息区
//    settextstyle(28, 0, _T("微软雅黑"));
//    outtextxy(320, 30, _T("好车主助手 - 图形界面"));
//    settextstyle(16, 0, _T("微软雅黑"));
//    outtextxy(320, 80, _T("提示：本界面为示范，点击左侧按钮执行对应操作（仅部分功能实现）。"));
//
//    for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
//
//    // 主事件循环
//    MOUSEMSG msg;
//    int running = 1;
//    while (running) {
//        if (MouseHit()) {
//            msg = GetMouseMsg();
//            if (msg.uMsg == WM_LBUTTONDOWN) {
//                int mx = msg.x;
//                int my = msg.y;
//                if (pt_in_button(mx, my, &btns[0])) {
//                    // 车辆管理：显示车辆列表
//                    gui_list_cars(ctx);
//                    // 重绘主界面
//                    cleardevice();
//                    setbkcolor(WHITE);
//                    outtextxy(320, 30, _T("好车主助手 - 图形界面"));
//                    outtextxy(320, 80, _T("提示：本界面为示范，点击左侧按钮执行对应操作（仅部分功能实现）。"));
//                    for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
//                } else if (pt_in_button(mx, my, &btns[1])) {
//                    // 保险管理 - 仅显示保单列表
//                    cleardevice();
//                    settextstyle(20, 0, _T("微软雅黑"));
//                    outtextxy(20, 10, _T("保单列表（图形界面展示）"));
//                    if (!ctx || ctx->policy_count == 0) {
//                        outtextxy(20, 50, _T("无保单记录。按任意位置返回。"));
//                        while (1) { if (MouseHit()) { msg = GetMouseMsg(); if (msg.uMsg==WM_LBUTTONDOWN) break; } Sleep(10);}                        
//                    } else {
//                        char buf[256];
//                        for (int i = 0; i < ctx->policy_count; ++i) {
//                            const Policy* p = &ctx->policies[i];
//                            snprintf(buf, sizeof(buf), "%d. %s  车牌:%s  投保人:%s  保额:%.2f  保费:%.2f",
//                                i+1, p->policy_id, p->plate, p->owner, p->coverage_limit, p->premium);
//                            outtextxy(20, 50 + i * 22, buf);
//                        }
//                        outtextxy(20, 50 + ctx->policy_count * 22 + 10, _T("点击任意位置返回"));
//                        while (1) { if (MouseHit()) { msg = GetMouseMsg(); if (msg.uMsg==WM_LBUTTONDOWN) break; } Sleep(10);}                        
//                    }
//                    // 重绘主界面
//                    cleardevice();
//                    outtextxy(320, 30, _T("好车主助手 - 图形界面"));
//                    outtextxy(320, 80, _T("提示：本界面为示范，点击左侧按钮执行对应操作（仅部分功能实现）。"));
//                    for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
//                } else if (pt_in_button(mx, my, &btns[2])) {
//                    // 用户管理：显示当前登录用户
//                    cleardevice();
//                    settextstyle(20, 0, _T("微软雅黑"));
//                    outtextxy(20, 10, _T("用户信息"));
//                    if (!ctx || ctx->current_user[0] == '\0') {
//                        outtextxy(20, 50, _T("当前未登录。"));
//                    } else {
//                        char buf[80];
//                        snprintf(buf, sizeof(buf), "当前用户：%s", ctx->current_user);
//                        outtextxy(20, 50, buf);
//                    }
//                    outtextxy(20, 120, _T("点击任意位置返回"));
//                    while (1) { if (MouseHit()) { msg = GetMouseMsg(); if (msg.uMsg==WM_LBUTTONDOWN) break; } Sleep(10);}                        
//                    cleardevice();
//                    outtextxy(320, 30, _T("好车主助手 - 图形界面"));
//                    outtextxy(320, 80, _T("提示：本界面为示范，点击左侧按钮执行对应操作（仅部分功能实现）。"));
//                    for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
//                } else if (pt_in_button(mx, my, &btns[3])) {
//                    running = 0; // 退出
//                    break;
//                }
//            }
//        }
//        Sleep(10);
//    }
//
//    // 关闭图形窗口
//    closegraph();
//}
