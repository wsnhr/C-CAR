#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"
//1.按钮与点击测试
//2.弹出输入文本框
//3.上下文与状态
/*
 * gui.c —— GUI 总入口与主事件循环
 *
 * 本文件处在 GUI 调用链的最上层，只负责三件事：
 * 1. 根据 state->screen 判断当前应当绘制首页、车辆页还是保单理赔页；
 * 2. 从 EasyX 消息队列取得鼠标点击，并把坐标交给当前页面做命中检测；
 * 3. 把命中检测得到的“操作编号”交给对应页面的 handle_*_action 函数。
 *
 * 具体控件怎么画、输入框怎么收集数据、车辆和保单如何修改，都由其他文件
 * 完成。这样 gui_run 不会和某一个业务功能绑死，增加页面时只需增加一个分支。
 */

/*
 * 绘制当前页面。
 *
 * ctx   ：只读的应用数据，绘制函数从中取得当前用户、车辆和保单等信息。
 * state ：只读的界面状态，screen 字段决定当前页面。
 *
 * 本函数只做页面分派，不直接画按钮。真正的绘制分别由 draw_home、
 * draw_cars 和 draw_insurance 完成，这三个函数处于同一层级。
 */
static void draw_ui(const AppContext *ctx, const GuiState *state)
{
    /* EasyX 的绘图状态具有持续性：先设置统一背景色，再清空上一帧，避免旧页面
       残留。cleardevice 使用当前背景色填满整个绘图窗口。 */
    setbkcolor(RGB(236, 242, 248));
    cleardevice();
    /* screen 是一级页面枚举。这里只允许一个分支执行，所以三个页面绘制函数
       是并列关系，不是互相包含关系。 */
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
    /* 批量绘图期间，前面的绘制先进入后台缓冲；FlushBatchDraw 一次性显示
       完整画面，减少控件逐个出现造成的闪烁。 */
    FlushBatchDraw();
}

/*
 * GUI 生命周期：加载数据 -> 初始化状态和窗口 -> 循环绘制/接收点击 -> 关闭。
 * running 是循环开关，首页的“退出”操作通过 int* 修改它。
 */
void gui_run(AppContext *ctx)
{
    /* 程序启动时先把各文本文件恢复到同一个 AppContext。保单加载后立即根据
       今天更新 active，理赔随后加载；这些函数失败时对应计数会保持为 0。 */
    load_users(ctx);
    load_cars(ctx);
    load_policies(ctx);
    update_policy_active_status(ctx);
    load_claims(ctx);

    /* GuiState 只描述界面，不保存车辆等业务对象。先全体清零，再为不能简单
       依赖 0 含义的成员指定明确初值。 */
    GuiState state;
    memset(&state, 0, sizeof(state)); /* 先把页码、筛选等成员全部清零。 */
    state.screen = SCREEN_HOME;
    state.insurance_view = VIEW_POLICY_LIST;
    set_message(&state, L"请从左侧选择功能");

    /* initgraph 创建 EasyX 窗口；BeginBatchDraw 开启双缓冲模式。 */
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, 0); /* 不显示控制台窗口 */
    BeginBatchDraw();
    int running = 1;
    while (running)
    {
        draw_ui(ctx, &state);
        /*
         * ExMessage 是 EasyX 的事件结构体。peekmessage 成功后，msg.message
         * 保存事件类型，msg.x 和 msg.y 保存鼠标相对于绘图窗口左上角的坐标。
         */
        ExMessage msg;
        /*
         * peekmessage(&msg, EM_MOUSE, 1) 的三个实参分别表示：
         *   &msg     —— 让 EasyX 把取到的事件写入变量 msg；
         *   EM_MOUSE —— 只关心鼠标事件；
         *   1        —— 读出事件后将它从消息队列移除，避免同一次点击被重复处理。
         *
         * && 具有短路特性：只有确实取到鼠标事件，程序才读取 msg.message。
         * WM_LBUTTONDOWN 表示鼠标左键刚被按下，因此移动鼠标或松开按键不会
         * 进入下面的点击处理代码。
         */
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN)
        {
            /* 第一阶段只做命中检测：把坐标转换为当前页面的操作编号。只调用
               当前页面的 hit_test，避免同一坐标误命中其他页面按钮。 */
            int action = 0;
            /* 第二阶段做操作分派：把编号交给与刚才同一页面的处理器。处理器
               可能修改 ctx、页码或 screen，变化会在下一轮 draw_ui 中出现。 */
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
    /* running 变成 0 后结束双缓冲并销毁图形窗口，gui_run 随后返回 main。 */
    EndBatchDraw();
    closegraph();
}
