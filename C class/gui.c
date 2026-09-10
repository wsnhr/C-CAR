#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui.c —— GUI 总入口与主事件循环
 *
 * 本文件决定“当前画哪个页面”和“鼠标点击交给哪个页面处理”。具体绘制、
 * 输入框和业务操作分散在 gui_common/gui_home/gui_car/gui_insurance 中。
 */

/* 根据 state->screen 构造当前页面按钮并调用对应 draw_* 函数。 */
static void draw_ui(const AppContext *ctx, const GuiState *state)
{
    setbkcolor(RGB(236, 242, 248));
    cleardevice();
    if (state->screen == SCREEN_HOME)
    {
        draw_home(ctx, state);
    }
    else if (state->screen == SCREEN_CARS)
    {
        draw_cars(ctx, state);
    }
    else
    {
        draw_insurance(ctx, state);
    }
    FlushBatchDraw();//？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？
}

/*
 * GUI 生命周期：加载数据 -> 初始化状态和窗口 -> 循环绘制/接收点击 -> 关闭。
 * running 是循环开关，首页的“退出”操作通过 int* 修改它。
 */
void gui_run(AppContext *ctx)
{
    load_users(ctx);
    load_cars(ctx);
    load_policies(ctx);
    // 在加载保单后立即更新保单状态（自动过期）
    update_policy_active_status(ctx);//？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？
    load_claims(ctx);

    GuiState state;
    memset(&state, 0, sizeof(state)); /* 先把页码、筛选等成员全部清零。 *///？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？
    state.screen = SCREEN_HOME;
    state.insurance_view = VIEW_POLICY_LIST;
    set_message(&state, L"请从左侧选择功能");//？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？

    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, 0); /* 不显示控制台窗口 */
    BeginBatchDraw();
    int running = 1;
    while (running)
    {
        draw_ui(ctx, &state);
        /* ExMessage 是 EasyX 的事件结构体，包含事件种类和鼠标坐标。 */
        ExMessage msg;
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN)
        {
            /* 先命中测试得到操作编号，再把编号交给当前页面的处理器。 */
            int action = 0;
            if (state.screen == SCREEN_HOME)
                action = hit_test_home(ctx, msg.x, msg.y);
            else if (state.screen == SCREEN_CARS)
                action = hit_test_cars(ctx, msg.x, msg.y);
            else
                action = hit_test_insurance(ctx, msg.x, msg.y);
            if (state.screen == SCREEN_HOME)
                handle_home_action(ctx, &state, action, &running);
            else if (state.screen == SCREEN_CARS)
                handle_car_action(ctx, &state, action);
            else
                handle_insurance_action(ctx, &state, action);
        }
    }
    EndBatchDraw();
    closegraph();
}
