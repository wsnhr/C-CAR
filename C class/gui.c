#define _CRT_SECURE_NO_WARNINGS
#include "gui.h"

// EasyX GUI for C-CAR
// 说明：
// - 本文件使用 EasyX 绘图库（graphics.h）实现一个简单的图形化界面，包含主菜单、车辆列表、保单列表、用户信息展示等。
// - 为便于维护，代码中加入了较多注释，描述各函数的职责与交互流程。
// - 注意：项目需要配置 EasyX 的 include/lib 路径，且在 Windows 环境下使用。

#include <graphics.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>

// 按钮结构：用于在主界面绘制并处理鼠标点击
typedef struct {
    int x, y, w, h;       // 按钮的矩形区域（左上角 x,y，宽 w，高 h）
    const char* label;    // 显示的文本（UTF-8 编码经 EasyX 输出时会被转换）
} Button;

// draw_button
// 绘制一个带边框和填充色的按钮，文本使用中文字体显示
static void draw_button(const Button* b) {
    // 按钮背景和边框
    setlinecolor(RGB(50, 50, 50));
    setfillcolor(RGB(200, 220, 255));
    bar(b->x, b->y, b->x + b->w, b->y + b->h);
    setlinecolor(BLACK);
    rectangle(b->x, b->y, b->x + b->w, b->y + b->h);

    // 按钮文字（注：EasyX 的 _T 宏用于宽字符）
    settextstyle(20, 0, _T("微软雅黑"));
    settextcolor(BLACK);
    int tx = b->x + 18; // 留边距
    int ty = b->y + (b->h - 20) / 2;
    outtextxy(tx, ty, b->label);
}

// pt_in_button
// 检查像素点(px,py)是否位于按钮区域内
static int pt_in_button(int px, int py, const Button* b) {
    return px >= b->x && px <= b->x + b->w && py >= b->y && py <= b->y + b->h;
}

// get_click_position
// 非阻塞方式检测鼠标左键点击并返回窗口客户区坐标
// 返回 1 表示检测到一次点击并将坐标写入 out_x/out_y，否则返回 0
static int get_click_position(int* out_x, int* out_y) {
    // 使用 Win32 API 的 GetAsyncKeyState 与 GetCursorPos
    // 优点：不依赖 EasyX 的 MOUSEMSG 类型，便于移植与避免头文件差异
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        POINT pt;
        if (GetCursorPos(&pt)) {
            HWND hwnd = GetForegroundWindow();
            ScreenToClient(hwnd, &pt);
            *out_x = (int)pt.x;
            *out_y = (int)pt.y;
            // 等待左键释放以避免连续触发
            while (GetAsyncKeyState(VK_LBUTTON) & 0x8000) Sleep(10);
            return 1;
        }
    }
    return 0;
}

// wait_click
// 阻塞等待一次鼠标左键点击，返回点击位置（窗口客户区坐标）
static void wait_click(int* out_x, int* out_y) {
    while (!get_click_position(out_x, out_y)) Sleep(10);
}

// gui_list_cars
// 图形界面中展示车辆列表，支持分页显示（每页最多 20 条）
// 参数：ctx - 应用上下文，包含车辆数组与数量
static void gui_list_cars(const AppContext* ctx) {
    // 清屏并设置基础文本样式
    cleardevice();
    setbkcolor(WHITE);
    settextstyle(20, 0, _T("微软雅黑"));
    settextcolor(BLACK);
    outtextxy(20, 10, _T("车辆列表（图形界面）"));

    int startY = 50;
    int lineH = 26; // 行高，便于换行显示
    char buf[512];

    if (!ctx || ctx->car_count == 0) {
        outtextxy(20, startY, _T("当前没有车辆记录。"));
        outtextxy(20, startY + 40, _T("点击任意位置返回主菜单。"));
        int rx, ry; wait_click(&rx, &ry);
        return;
    }

    // 列表显示；若条目过多则分页
    for (int i = 0; i < ctx->car_count; ++i) {
        const Car* c = &ctx->cars[i];
        // 格式化为中文描述：序号、车牌、品牌、型号、违章等级、购买日期、购入价
        snprintf(buf, sizeof(buf), "%d. 车牌:%s  品牌:%s  型号:%s  违章:%d  购入日期:%04d-%02d-%02d  价格:%.2f元",
            i + 1, c->plate, c->brand, c->model, (int)c->violation,
            c->purchase_date.year, c->purchase_date.month, c->purchase_date.day,
            c->purchase_price);
        outtextxy(20, startY + i * lineH, buf);

        // 到达单页上限后提示翻页
        if ((i + 1) % 20 == 0 && i + 1 < ctx->car_count) {
            outtextxy(20, startY + 21 * lineH, _T("点击任意位置显示下一页..."));
            int rx, ry; wait_click(&rx, &ry);
            cleardevice();
            outtextxy(20, 10, _T("车辆列表（图形界面）"));
        }
    }

    // 显示返回提示并等待点击
    outtextxy(20, startY + ((ctx->car_count % 20) + 2) * lineH, _T("点击任意位置返回主菜单"));
    int rx, ry; wait_click(&rx, &ry);
}

// start_gui
// 主入口：创建 EasyX 窗口并进入事件循环，用户通过鼠标点击左侧按钮进行切换
void start_gui(AppContext* ctx) {
    // 1) 初始化图形窗口，大小 900x640 更适合中文显示
    initgraph(900, 640);
    setbkcolor(WHITE);
    cleardevice();

    // 2) 定义左侧按钮：车辆管理、保险管理、用户管理、退出
    Button btns[4] = {
        {40, 120, 260, 60, "车辆管理"},
        {40, 210, 260, 60, "保单管理"},
        {40, 300, 260, 60, "用户信息"},
        {40, 390, 260, 60, "退出程序"}
    };

    // 3) 绘制主界面静态文本与按钮
    settextstyle(28, 0, _T("微软雅黑"));
    outtextxy(340, 30, _T("好车主助手 - 图形界面"));
    settextstyle(16, 0, _T("微软雅黑"));
    outtextxy(340, 80, _T("说明：此界面为演示用途。使用鼠标点击左侧按钮查看对应数据，关闭窗口返回控制台。"));

    for (int i = 0; i < 4; ++i) draw_button(&btns[i]);

    // 4) 事件循环：基于鼠标点击坐标判断按钮触发
    int running = 1;
    while (running) {
        int mx, my;
        if (get_click_position(&mx, &my)) {
            if (pt_in_button(mx, my, &btns[0])) {
                // 点击：车辆管理 -> 展示车辆列表
                gui_list_cars(ctx);
                // 返回后重绘主界面
                cleardevice();
                setbkcolor(WHITE);
                settextstyle(28, 0, _T("微软雅黑"));
                outtextxy(340, 30, _T("好车主助手 - 图形界面"));
                settextstyle(16, 0, _T("微软雅黑"));
                outtextxy(340, 80, _T("说明：此界面为演示用途。使用鼠标点击左侧按钮查看对应数据，关闭窗口返回控制台。"));
                for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
            } else if (pt_in_button(mx, my, &btns[1])) {
                // 点击：保单管理 -> 展示保单列表
                cleardevice();
                settextstyle(20, 0, _T("微软雅黑"));
                outtextxy(20, 10, _T("保单列表（图形界面）"));
                if (!ctx || ctx->policy_count == 0) {
                    outtextxy(20, 50, _T("当前没有保单记录。点击任意位置返回。"));
                    int rx, ry; wait_click(&rx, &ry);
                } else {
                    char buf[512];
                    for (int i = 0; i < ctx->policy_count; ++i) {
                        const Policy* p = &ctx->policies[i];
                        snprintf(buf, sizeof(buf), "%d. 保单号:%s  车牌:%s  投保人:%s  保额:%.2f元  年保费:%.2f元",
                            i + 1, p->policy_id, p->plate, p->owner, p->coverage_limit, p->premium);
                        outtextxy(20, 50 + i * 24, buf);
                    }
                    outtextxy(20, 50 + ctx->policy_count * 24 + 12, _T("点击任意位置返回主菜单"));
                    int rx, ry; wait_click(&rx, &ry);
                }
                // 重绘主界面
                cleardevice();
                settextstyle(28, 0, _T("微软雅黑"));
                outtextxy(340, 30, _T("好车主助手 - 图形界面"));
                settextstyle(16, 0, _T("微软雅黑"));
                outtextxy(340, 80, _T("说明：此界面为演示用途。使用鼠标点击左侧按钮查看对应数据，关闭窗口返回控制台。"));
                for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
            } else if (pt_in_button(mx, my, &btns[2])) {
                // 点击：用户信息 -> 展示当前登录用户
                cleardevice();
                settextstyle(20, 0, _T("微软雅黑"));
                outtextxy(20, 10, _T("当前用户信息"));
                if (!ctx || ctx->current_user[0] == '\0') {
                    outtextxy(20, 50, _T("当前未登录。如需使用车辆或保单功能，请先通过控制台登录。"));
                } else {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "当前登录用户：%s", ctx->current_user);
                    outtextxy(20, 50, buf);
                    outtextxy(20, 80, _T("说明：当前示例不支持在图形界面内修改用户信息，请返回控制台进行操作。"));
                }
                outtextxy(20, 140, _T("点击任意位置返回主菜单"));
                int rx, ry; wait_click(&rx, &ry);

                // 重绘主界面
                cleardevice();
                settextstyle(28, 0, _T("微软雅黑"));
                outtextxy(340, 30, _T("好车主助手 - 图形界面"));
                settextstyle(16, 0, _T("微软雅黑"));
                outtextxy(340, 80, _T("说明：此界面为演示用途。使用鼠标点击左侧按钮查看对应数据，关闭窗口返回控制台。"));
                for (int i = 0; i < 4; ++i) draw_button(&btns[i]);
            } else if (pt_in_button(mx, my, &btns[3])) {
                // 退出按钮：结束事件循环，关闭 EasyX 窗口
                running = 0;
                break;
            }
        }
        Sleep(10); // 减少 CPU 占用
    }

    // 关闭图形窗口并返回调用者（控制台）
    closegraph();
}
