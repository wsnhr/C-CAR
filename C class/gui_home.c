#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui_home.c —— 首页绘制、注册登录和页面导航
 * draw_home 只负责显示；handle_home_action 负责响应操作；hit_test_home
 * 只负责判断鼠标落在哪个按钮。三者分离后职责更清楚。
 */

static const Button HOME_GUEST_BUTTONS[] = {{{20, 120, 200, 165}, L"注册", HOME_REGISTER},
                                            {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
                                            {{20, 240, 200, 285}, L"忘记密码", HOME_FORGOT_PASSWORD},
                                            {{20, 300, 200, 345}, L"退出", HOME_EXIT}};

static const Button HOME_USER_BUTTONS[] = {{{20, 120, 200, 165}, L"登出", HOME_LOGOUT},
                                           {{20, 180, 200, 225}, L"车辆管理", HOME_CARS},
                                           {{20, 240, 200, 285}, L"保单管理", HOME_INSURANCE},
                                           {{20, 300, 200, 345}, L"退出", HOME_EXIT}};

static const Button HOME_ADMIN_BUTTONS[] = {{{20, 120, 200, 165}, L"登出", HOME_LOGOUT},
                                             {{20, 180, 200, 225}, L"车辆管理", HOME_CARS},
                                             {{20, 240, 200, 285}, L"保单管理", HOME_INSURANCE},
                                             {{20, 300, 200, 345}, L"删除用户", HOME_DELETE_USER},
                                             {{20, 360, 200, 405}, L"退出", HOME_EXIT}};

/*
 * 首页统计只服务于界面展示，因此结构体定义在 gui_home.c 内部，而不放入
 * app_types.h。这样业务模块不需要知道“统计卡片”这种界面概念。
 */
typedef struct HomeStatistics
{
    int registered_users; /* 已注册普通用户数量；内置 admin 不在 users 数组中。 */
    int registered_cars;  /* 当前登记的车辆总数。 */
    int total_policies;   /* 全部保单数量。 */
    int active_policies;  /* active 为 1 的有效保单数量。 */
    int total_claims;     /* 全部理赔数量。 */
    int pending_claims;   /* 等待管理员审核。 */
    int approved_claims;  /* 已审核通过但还没有结案。 */
    int settled_claims;   /* 已经结案。 */
    int rejected_claims;  /* 已经驳回。 */
    int cancelled_claims; /* 申请人在审核前撤销。 */
} HomeStatistics;

/* 绘制和命中检测都通过这里取得同一份按钮定义，避免坐标不一致。 */
static const Button *home_buttons(const AppContext *ctx, int *count)
{
    if (!app_is_logged_in(ctx))
    {
        *count = (int)(sizeof(HOME_GUEST_BUTTONS) / sizeof(HOME_GUEST_BUTTONS[0]));
        return HOME_GUEST_BUTTONS;
    }
    if (app_is_admin(ctx))
    {
        *count = (int)(sizeof(HOME_ADMIN_BUTTONS) / sizeof(HOME_ADMIN_BUTTONS[0]));
        return HOME_ADMIN_BUTTONS;
    }
    *count = (int)(sizeof(HOME_USER_BUTTONS) / sizeof(HOME_USER_BUTTONS[0]));
    return HOME_USER_BUTTONS;
}

/*
 * 顺序扫描保单和理赔数组，计算管理员首页需要的统计数字。
 * const AppContext* 表示函数只能读取上下文，编译器会阻止这里误改业务数据。
 */
static HomeStatistics collect_home_statistics(const AppContext *ctx)
{
    HomeStatistics stats = {0};
    int i;

    if (!ctx)
        return stats;

    stats.registered_users = ctx->user_count;
    stats.registered_cars = ctx->car_count;
    stats.total_policies = ctx->policy_count;
    stats.total_claims = ctx->claim_count;

    for (i = 0; i < ctx->policy_count; ++i)
    {
        if (ctx->policies[i].active)
            ++stats.active_policies;
    }

    for (i = 0; i < ctx->claim_count; ++i)
    {
        switch (ctx->claims[i].status)
        {
        case CLAIM_PENDING:
            ++stats.pending_claims;
            break;
        case CLAIM_APPROVED:
            ++stats.approved_claims;
            break;
        case CLAIM_SETTLED:
            ++stats.settled_claims;
            break;
        case CLAIM_REJECTED:
            ++stats.rejected_claims;
            break;
        case CLAIM_CANCELLED:
            ++stats.cancelled_claims;
            break;
        default:
            break;
        }
    }

    return stats;
}

/*
 * 绘制带轻微阴影的圆角面板。先把阴影向右下偏移 3 像素，再绘制白色主体，
 * 即可在不使用图片和透明度的情况下得到简单的层次感。
 */
static void draw_home_panel(int left, int top, int right, int bottom)
{
    setlinecolor(RGB(226, 232, 240));
    setfillcolor(RGB(226, 232, 240));
    solidroundrect(left + 3, top + 3, right + 3, bottom + 3, 14, 14);
    setlinecolor(RGB(218, 226, 236));
    setfillcolor(RGB(255, 255, 255));
    solidroundrect(left, top, right, bottom, 14, 14);
}

/*
 * 使用普通 C 函数创建 RECT，避免在调用处使用 C++ 初始化语法。
 * RECT 的四个成员依次表示矩形左、上、右、下边界。
 */
static RECT make_home_rect(int left, int top, int right, int bottom)
{
    RECT rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    return rect;
}

/* 在指定矩形中绘制一行文字，避免每个卡片重复设置字体、颜色和对齐方式。 */
static void draw_home_text(const wchar_t *text, RECT rect, int size, COLORREF color, UINT format)
{
    setbkmode(TRANSPARENT);
    settextstyle(size, 0, L"Microsoft YaHei UI");
    settextcolor(color);
    draw_text_rect(text, rect, format);
}

/*
 * 首页使用深色侧边栏，所以不能复用其他页面的浅色 draw_button。
 * Button.rect 同时用于绘制和点击检测，因此改变外观不会造成点击位置错位。
 */
static void draw_home_navigation_button(const Button *button)
{
    COLORREF fill = RGB(43, 57, 87);
    COLORREF border = RGB(59, 76, 111);
    COLORREF text = RGB(235, 241, 250);
    RECT label_rect;

    if (button->id == HOME_DELETE_USER)
    {
        fill = RGB(75, 48, 61);
        border = RGB(130, 68, 83);
        text = RGB(255, 214, 220);
    }
    else if (button->id == HOME_EXIT)
    {
        fill = RGB(37, 48, 72);
        border = RGB(75, 88, 116);
        text = RGB(195, 205, 222);
    }

    setlinecolor(border);
    setfillcolor(fill);
    solidroundrect(button->rect.left, button->rect.top, button->rect.right, button->rect.bottom, 12,
                   12);

    /* 左侧短色条用于提示这是可点击的导航项。 */
    setlinecolor(button->id == HOME_DELETE_USER ? RGB(239, 103, 115) : RGB(96, 165, 250));
    setfillcolor(button->id == HOME_DELETE_USER ? RGB(239, 103, 115) : RGB(96, 165, 250));
    solidroundrect(button->rect.left + 8, button->rect.top + 11, button->rect.left + 12,
                   button->rect.bottom - 11, 4, 4);

    label_rect = button->rect;
    label_rect.left += 26;
    draw_home_text(button->label, label_rect, 18, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 绘制首页左侧品牌区、登录状态、导航分组标题和底部版本说明。 */
static void draw_home_sidebar(const AppContext *ctx, const Button *buttons, int button_count)
{
    int i;
    wchar_t user_text[64] = {0};
    wchar_t status[96] = {0};
    RECT brand_rect = {52, 25, 205, 58};
    RECT brand_subtitle = {20, 62, 205, 86};
    RECT section_rect = {20, 92, 205, 116};
    RECT status_rect = {20, 600, 200, 632};
    RECT version_rect = {20, 646, 200, 676};

    setlinecolor(RGB(31, 42, 68));
    setfillcolor(RGB(31, 42, 68));
    solidrectangle(0, 0, 220, WINDOW_HEIGHT);

    /* 简单的字母标志由图形和文字组成，不依赖额外图片资源。 */
    setlinecolor(RGB(59, 130, 246));
    setfillcolor(RGB(59, 130, 246));
    solidroundrect(20, 25, 44, 53, 8, 8);
    draw_home_text(L"C", make_home_rect(20, 25, 44, 53), 18, RGB(255, 255, 255),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"好车主助手", brand_rect, 24, RGB(248, 250, 252),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"CAR MANAGEMENT", brand_subtitle, 12, RGB(137, 155, 187),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"功能导航", section_rect, 14, RGB(137, 155, 187),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    for (i = 0; i < button_count; ++i)
        draw_home_navigation_button(&buttons[i]);

    if (app_is_logged_in(ctx))
    {
        char_to_wchar(ctx->current_user, user_text, 64);
        _snwprintf_s(status, 96, _TRUNCATE, app_is_admin(ctx) ? L"● 管理员：%s" : L"● 用户：%s",
                     user_text);
        draw_home_text(status, status_rect, 16, RGB(110, 231, 183),
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        draw_home_text(L"● 当前未登录", status_rect, 16, RGB(148, 163, 184),
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    draw_home_text(L"本地数据管理系统", version_rect, 13, RGB(112, 130, 162),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 绘制右侧页面标题和最近一次操作消息。 */
static void draw_home_header(const AppContext *ctx, const GuiState *state)
{
    const wchar_t *title;
    const wchar_t *subtitle;
    RECT title_rect = {260, 18, 640, 57};
    RECT subtitle_rect = {260, 53, 690, 80};
    RECT message_rect = {690, 27, 970, 76};

    if (!app_is_logged_in(ctx))
    {
        title = L"欢迎使用好车主助手";
        subtitle = L"车辆、保险与理赔一体化管理";
    }
    else if (app_is_admin(ctx))
    {
        title = L"管理员控制台";
        subtitle = L"系统数据与待处理业务概览";
    }
    else
    {
        title = L"欢迎回来";
        subtitle = L"管理您的车辆、保单与理赔信息";
    }

    draw_home_text(title, title_rect, 30, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(subtitle, subtitle_rect, 16, RGB(100, 116, 139),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    setlinecolor(RGB(191, 219, 254));
    setfillcolor(RGB(239, 246, 255));
    solidroundrect(message_rect.left, message_rect.top, message_rect.right, message_rect.bottom, 12,
                   12);
    message_rect.left += 14;
    message_rect.right -= 12;
    draw_home_text(state->message[0] ? state->message : L"请从左侧选择功能", message_rect, 15,
                   RGB(37, 99, 160), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/* 绘制管理员顶部的一张统计卡片。value 是大号数字，note 是补充说明。 */
static void draw_stat_card(int left, int top, int right, int bottom, const wchar_t *label, int value,
                           const wchar_t *note, COLORREF accent)
{
    wchar_t number[24] = {0};
    RECT label_rect = {left + 16, top + 13, right - 12, top + 42};
    RECT value_rect = {left + 16, top + 43, right - 12, top + 87};
    RECT note_rect = {left + 16, top + 91, right - 12, bottom - 8};

    draw_home_panel(left, top, right, bottom);
    setlinecolor(accent);
    setfillcolor(accent);
    solidroundrect(left + 15, top + 13, left + 21, top + 35, 5, 5);
    label_rect.left += 16;
    draw_home_text(label, label_rect, 16, RGB(71, 85, 105),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    _snwprintf_s(number, 24, _TRUNCATE, L"%d", value);
    draw_home_text(number, value_rect, 32, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(note, note_rect, 13, RGB(100, 116, 139),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 管理员专属首页：四张关键指标卡片，下方是理赔状态和动态管理提醒。 */
static void draw_admin_dashboard(const AppContext *ctx)
{
    HomeStatistics stats = collect_home_statistics(ctx);
    wchar_t policy_note[40] = {0};
    wchar_t pending_note[40] = {0};
    wchar_t line[96] = {0};
    int warning_y = 358;

    _snwprintf_s(policy_note, 40, _TRUNCATE, L"全部保单 %d", stats.total_policies);
    _snwprintf_s(pending_note, 40, _TRUNCATE,
                 stats.pending_claims > 0 ? L"需要管理员处理" : L"当前没有待办");

    draw_stat_card(260, 110, 428, 245, L"注册用户", stats.registered_users, L"普通用户总数",
                   RGB(59, 130, 246));
    draw_stat_card(440, 110, 608, 245, L"登记车辆", stats.registered_cars, L"系统车辆总数",
                   RGB(14, 165, 233));
    draw_stat_card(620, 110, 788, 245, L"有效保单", stats.active_policies, policy_note,
                   RGB(34, 197, 94));
    draw_stat_card(800, 110, 968, 245, L"待审理赔", stats.pending_claims, pending_note,
                   stats.pending_claims > 0 ? RGB(245, 158, 11) : RGB(34, 197, 94));

    draw_home_panel(260, 266, 608, 638);
    draw_home_text(L"理赔状态概览", make_home_rect(282, 282, 580, 320), 21, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"当前系统全部理赔的处理进度", make_home_rect(282, 316, 580, 342), 14,
                   RGB(100, 116, 139), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    _snwprintf_s(line, 96, _TRUNCATE, L"全部理赔                         %d", stats.total_claims);
    draw_home_text(line, make_home_rect(282, 358, 582, 398), 17, RGB(51, 65, 85),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    _snwprintf_s(line, 96, _TRUNCATE, L"等待审核                         %d", stats.pending_claims);
    draw_home_text(line, make_home_rect(282, 405, 582, 445), 17, RGB(180, 83, 9),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    _snwprintf_s(line, 96, _TRUNCATE, L"审核通过                         %d", stats.approved_claims);
    draw_home_text(line, make_home_rect(282, 452, 582, 492), 17, RGB(37, 99, 235),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    _snwprintf_s(line, 96, _TRUNCATE, L"结案 / 驳回 / 撤销       %d / %d / %d",
                 stats.settled_claims, stats.rejected_claims, stats.cancelled_claims);
    draw_home_text(line, make_home_rect(282, 499, 582, 539), 17, RGB(51, 65, 85),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"数据会随车辆、保单和理赔操作自动更新",
                   make_home_rect(282, 577, 582, 612), 13,
                   RGB(100, 116, 139), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_home_panel(625, 266, 968, 638);
    draw_home_text(L"管理提醒", make_home_rect(647, 282, 940, 320), 21, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"根据当前数据自动生成", make_home_rect(647, 316, 940, 342), 14,
                   RGB(100, 116, 139), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (stats.pending_claims > 0)
    {
        _snwprintf_s(line, 96, _TRUNCATE, L"● 有 %d 条理赔等待审核", stats.pending_claims);
        draw_home_text(line, make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(180, 83, 9),
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        warning_y += 46;
    }
    else
    {
        draw_home_text(L"● 当前没有待审核理赔",
                       make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(22, 163, 74), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        warning_y += 46;
    }

    if (stats.registered_users >= MAX_USERS * 8 / 10)
    {
        draw_home_text(L"● 用户数量已接近容量上限",
                       make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(190, 70, 60), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        warning_y += 46;
    }
    if (stats.registered_cars >= MAX_CARS * 8 / 10)
    {
        draw_home_text(L"● 车辆数量已接近容量上限",
                       make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(190, 70, 60), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        warning_y += 46;
    }
    if (stats.total_policies >= MAX_POLICIES * 8 / 10)
    {
        draw_home_text(L"● 保单数量已接近容量上限",
                       make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(190, 70, 60), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        warning_y += 46;
    }
    if (warning_y <= 404)
    {
        draw_home_text(L"● 用户、车辆和保单容量正常",
                       make_home_rect(647, warning_y, 944, warning_y + 34), 16,
                       RGB(71, 85, 105), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* 提醒最多占用上方四行；处理建议固定放在更低位置，避免文字互相覆盖。 */
    draw_home_text(L"处理建议", make_home_rect(647, 535, 940, 562), 16, RGB(51, 65, 85),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(stats.pending_claims > 0 ? L"进入“保单管理”审核待处理理赔"
                                            : L"当前业务状态正常，请继续保持",
                   make_home_rect(647, 565, 940, 610), 14, RGB(100, 116, 139),
                   DT_LEFT | DT_VCENTER | DT_WORDBREAK);
}

/* 普通用户首页只介绍本人可用功能，不泄露系统的全局统计数据。 */
static void draw_user_dashboard(const AppContext *ctx)
{
    wchar_t username[64] = {0};
    wchar_t welcome[128] = {0};

    char_to_wchar(ctx->current_user, username, 64);
    _snwprintf_s(welcome, 128, _TRUNCATE, L"%s，欢迎回来", username);

    draw_home_panel(260, 110, 968, 260);
    draw_home_text(welcome, make_home_rect(290, 132, 930, 182), 28, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"您只能查看和管理本人名下的数据，所有操作都会经过权限检查。",
                   make_home_rect(290, 184, 930, 225), 16, RGB(100, 116, 139),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_home_panel(260, 282, 608, 610);
    draw_home_text(L"车辆管理", make_home_rect(287, 306, 580, 350), 23, RGB(37, 99, 235),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"登记本人车辆\n\n查看车辆列表和违章等级\n\n按多个条件查询车辆\n\n修改或删除车辆资料",
                   make_home_rect(287, 360, 575, 535), 16, RGB(71, 85, 105),
                   DT_LEFT | DT_WORDBREAK);
    draw_home_text(L"从左侧进入车辆管理  →", make_home_rect(287, 548, 575, 585), 15,
                   RGB(37, 99, 235), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_home_panel(625, 282, 968, 610);
    draw_home_text(L"保单与理赔", make_home_rect(652, 306, 940, 350), 23, RGB(22, 163, 74),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"查看本人车辆保单\n\n购买符合条件的保险\n\n提交理赔申请\n\n跟踪理赔处理状态",
                   make_home_rect(652, 360, 935, 535), 16, RGB(71, 85, 105),
                   DT_LEFT | DT_WORDBREAK);
    draw_home_text(L"从左侧进入保单管理  →", make_home_rect(652, 548, 935, 585), 15,
                   RGB(22, 163, 74), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 未登录首页使用欢迎语和三步流程，引导第一次使用系统的用户。 */
static void draw_guest_dashboard(void)
{
    draw_home_panel(260, 110, 968, 285);
    draw_home_text(L"让车辆与保险管理更简单", make_home_rect(292, 135, 930, 190), 30,
                   RGB(30, 41, 59), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"统一管理车辆资料、保险保单与理赔记录，数据保存在本地。",
                   make_home_rect(292, 193, 930, 236), 17, RGB(100, 116, 139),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"请先从左侧注册或登录", make_home_rect(292, 238, 930, 266), 15,
                   RGB(37, 99, 235), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_home_panel(260, 310, 484, 570);
    draw_home_text(L"01", make_home_rect(284, 329, 340, 370), 24, RGB(59, 130, 246),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"注册账户", make_home_rect(284, 375, 455, 414), 21, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"完成实名认证并创建自己的登录账户。",
                   make_home_rect(284, 424, 455, 510), 15,
                   RGB(100, 116, 139), DT_LEFT | DT_WORDBREAK);

    draw_home_panel(502, 310, 726, 570);
    draw_home_text(L"02", make_home_rect(526, 329, 582, 370), 24, RGB(14, 165, 233),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"登记车辆", make_home_rect(526, 375, 697, 414), 21, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"保存车辆型号、购入信息和违章等级。",
                   make_home_rect(526, 424, 697, 510), 15,
                   RGB(100, 116, 139), DT_LEFT | DT_WORDBREAK);

    draw_home_panel(744, 310, 968, 570);
    draw_home_text(L"03", make_home_rect(768, 329, 824, 370), 24, RGB(34, 197, 94),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"管理保险", make_home_rect(768, 375, 939, 414), 21, RGB(30, 41, 59),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_home_text(L"购买保单、提交理赔并查看处理进度。",
                   make_home_rect(768, 424, 939, 510), 15,
                   RGB(100, 116, 139), DT_LEFT | DT_WORDBREAK);

    draw_home_text(L"安全提示：密码和实名认证信息不会以明文形式保存",
                   make_home_rect(260, 592, 968, 630), 14, RGB(100, 116, 139),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/*
 * 首页总绘制函数只负责组合页面。身份分支放在这里，使访客、普通用户和管理员
 * 的内容彼此独立，后续修改某一种首页时不会影响另外两种。
 */
void draw_home(const AppContext *ctx, const GuiState *state)
{
    int count;
    const Button *buttons = home_buttons(ctx, &count);

    draw_home_sidebar(ctx, buttons, count);
    draw_home_header(ctx, state);
    if (!app_is_logged_in(ctx))
        draw_guest_dashboard();
    else if (app_is_admin(ctx))
        draw_admin_dashboard(ctx);
    else
        draw_user_dashboard(ctx);
}

/* 弹出输入框、验证长度，再调用 user.c 的统一注册接口。 */
static void handle_register(AppContext *ctx, GuiState *state)
{
    /* {0} 把数组全部初始化为 0，因此即使提前返回也始终是合法空字符串。 */
    char username[20] = {0}, password[20] = {0};
    char real_name[REAL_NAME_CAPACITY] = {0}, id_card[ID_CARD_CAPACITY] = {0};
    if (!prompt_char_field(L"注册", L"请输入用户名：", username, sizeof(username), ""))
        return;
    if (!validate_string_len(username, (int)sizeof(((User *)0)->username)))
    {
        set_message(state, L"用户名长度不合法（1-19 字符）");
        return;
    }
    if (!prompt_char_field(L"注册", L"请输入密码：", password, sizeof(password), ""))
        return;
    if (!validate_string_len(password, PASSWORD_INPUT_CAPACITY))
    {
        set_message(state, L"密码长度不合法（1-19 字符）");
        return;
    }
    if (!prompt_char_field(L"实名认证", L"请输入真实姓名（不能包含空格）：", real_name,
                           sizeof(real_name), ""))
        return;
    if (!validate_real_name(real_name))
    {
        set_message(state, L"真实姓名格式不合法");
        return;
    }
    if (!prompt_char_field(L"实名认证", L"请输入18位身份证号：", id_card,
                           sizeof(id_card), ""))
        return;
    if (!validate_chinese_id_card(id_card))
    {
        set_message(state, L"身份证号格式或校验码不正确");
        return;
    }
    if (register_user_account(ctx, username, password, real_name, id_card))
    {
        save_users(ctx);
        set_message(state, L"注册成功");
        append_operation_log(ctx, "ADD_USER", username, "registered");
    }
    else
    {
        if (ctx->user_count >= MAX_USERS)
            set_message(state, L"用户数量已达上限");
        else if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
            set_message(state, L"注册失败：用户名已存在");
        else
            set_message(state, L"注册失败");
    }
}

/*
 * 忘记密码的 GUI 流程只负责收集输入和显示结果：
 * 1. 先确认用户名属于可重置密码的普通账户；
 * 2. 收集姓名、身份证号和两次新密码，并做快速格式检查；
 * 3. 调用 user.c 验证实名哈希并替换密码哈希；
 * 4. 业务成功后调用 storage.c 保存 users.txt，再追加操作日志。
 *
 * 这里始终显示统一的“实名信息验证失败”，不告诉界面到底是姓名还是身份证
 * 不匹配，避免向尝试者泄露某一项信息是否正确。
 */
static void handle_forgot_password(AppContext *ctx, GuiState *state)
{
    char username[USERNAME_CAPACITY] = {0};
    char real_name[REAL_NAME_CAPACITY] = {0};
    char id_card[ID_CARD_CAPACITY] = {0};
    char new_password[PASSWORD_INPUT_CAPACITY] = {0};
    char confirm_password[PASSWORD_INPUT_CAPACITY] = {0};

    if (!prompt_char_field(L"忘记密码", L"请输入用户名：", username, sizeof(username), ""))
        return;
    if (!validate_string_len(username, USERNAME_CAPACITY) || strcmp(username, "admin") == 0 ||
        !user_exists(ctx, username))
    {
        set_message(state, L"无法验证该账户的实名信息");
        return;
    }
    if (!prompt_char_field(L"身份验证", L"请输入注册时的真实姓名：", real_name,
                           sizeof(real_name), ""))
        return;
    if (!prompt_char_field(L"身份验证", L"请输入注册时的身份证号：", id_card,
                           sizeof(id_card), ""))
        return;
    if (!validate_real_name(real_name) || !validate_chinese_id_card(id_card))
    {
        set_message(state, L"实名信息验证失败");
        return;
    }
    if (!prompt_char_field(L"重置密码", L"请输入新密码（1-19字符）：", new_password,
                           sizeof(new_password), ""))
        return;
    if (!validate_string_len(new_password, PASSWORD_INPUT_CAPACITY))
    {
        set_message(state, L"新密码长度不合法（1-19字符）");
        return;
    }
    if (!prompt_char_field(L"重置密码", L"请再次输入新密码：", confirm_password,
                           sizeof(confirm_password), ""))
        return;
    if (strcmp(new_password, confirm_password) != 0)
    {
        set_message(state, L"两次输入的新密码不一致");
        return;
    }
    if (!reset_user_password(ctx, username, real_name, id_card, new_password))
    {
        set_message(state, L"实名信息验证失败");
        return;
    }
    if (!save_users(ctx))
    {
        set_message(state, L"密码已修改，但保存失败，请勿关闭程序并联系管理员");
        return;
    }
    append_operation_log(ctx, "RESET_PASSWORD", username, "password reset after identity check");
    set_message(state, L"密码已重置，请使用新密码登录");
}

/* 收集用户名和密码并更新 current_user；密码不会显示在状态消息中。 */
static void handle_login(AppContext *ctx, GuiState *state)
{
    char username[20] = {0}, password[20] = {0};
    LoginResult result;
    int remaining_attempts = 0;
    int remaining_seconds = 0;
    wchar_t message[128] = {0};
    if (!prompt_char_field(L"登录", L"请输入用户名：", username, sizeof(username), ""))
        return;
    if (!validate_string_len(username, (int)sizeof(((User *)0)->username)))
    {
        set_message(state, L"用户名长度不合法");
        return;
    }
    if (!prompt_char_field(L"登录", L"请输入密码：", password, sizeof(password), ""))
        return;
    if (!validate_string_len(password, PASSWORD_INPUT_CAPACITY))
    {
        set_message(state, L"密码长度不合法");
        return;
    }
    result = login_user_account_guarded(ctx, username, password, &remaining_attempts,
                                        &remaining_seconds);
    if (result == LOGIN_RESULT_SUCCESS)
    {
        if (app_is_admin(ctx))
            set_message(state, L"以管理员身份登录");
        else
            set_message(state, L"登录成功");
        append_operation_log(ctx, "LOGIN", username, "user login");
        return;
    }
    if (result == LOGIN_RESULT_LOCKED)
    {
        _snwprintf_s(message, 128, _TRUNCATE, L"登录暂时锁定，请在 %d 秒后重试",
                     remaining_seconds);
        set_message(state, message);
        return;
    }
    _snwprintf_s(message, 128, _TRUNCATE, L"用户名或密码不匹配，还可以尝试 %d 次",
                 remaining_attempts);
    set_message(state, message);
}

/* 先记日志再清空 current_user，否则日志将无法记录是谁退出。 */
static void handle_logout(AppContext *ctx, GuiState *state)
{
    append_operation_log(ctx, "LOGOUT", ctx->current_user, "user logout");
    logout_current_user(ctx);
    set_message(state, L"已登出");
}

/*
 * switch 根据 HomeAction 枚举分派操作。running 是指针，HOME_EXIT 通过
 * 把 running 所指向的值改为 0，会同步修改 gui_run 中的原变量并结束外层事件循环。
 */
void handle_home_action(AppContext *ctx, GuiState *state, int action, int *running)
{
    switch (action)
    {
    case HOME_REGISTER:
        handle_register(ctx, state);
        break;
    case HOME_LOGIN:
        handle_login(ctx, state);
        break;
    case HOME_FORGOT_PASSWORD:
        handle_forgot_password(ctx, state);
        break;
    case HOME_LOGOUT:
        handle_logout(ctx, state);
        break;
    case HOME_CARS:
        state->screen = SCREEN_CARS;
        /* 每次进入车辆页都恢复默认范围：管理员看全部，普通用户只看本人。 */
        state->car_show_mine = app_is_admin(ctx) ? 0 : 1;
        memset(&state->car_search, 0, sizeof(state->car_search));
        state->car_search.vehicle_type = CAR_VEHICLE_TYPE_ANY;
        state->car_search.violation = CAR_VIOLATION_ANY;
        state->car_search_active = 0;
        state->car_page = 0;
        set_message(state, L"已进入车辆管理");
        break;
    case HOME_INSURANCE:
        update_policy_active_status(ctx);
        state->screen = SCREEN_INSURANCE;
        state->insurance_view = VIEW_POLICY_LIST;
        state->policy_page = 0;
        set_message(state, L"已进入保单管理");
        break;
    case HOME_DELETE_USER: {
        /* case 后增加一对花括号，允许在这个分支内部声明局部变量。 */
        char username[20] = {0};
        wchar_t username_text[64] = {0};
        wchar_t confirm_message[512] = {0};
        if (!app_is_admin(ctx))
        {
            set_message(state, L"只有管理员可删除用户");
            break;
        }
        if (!prompt_char_field(L"删除用户", L"请输入要删除的用户名：", username, sizeof(username),
                               ""))
            break;
        if (!user_exists(ctx, username))
        {
            set_message(state, L"未找到该用户");
            break;
        }
        char_to_wchar(username, username_text, 64);
        _snwprintf_s(confirm_message, 512, _TRUNCATE,
                     L"确定删除用户 %s 吗？\n\n警告：该用户的车辆、保单和理赔记录都会被级联删除。\n\n"
                     L"此操作无法撤销，请确认已经核对用户名。",
                     username_text);
        if (!show_confirm_dialog(L"删除用户", confirm_message, L"确认删除", 1))
        {
            set_message(state, L"已取消删除用户");
            break;
        }

        if (remove_user_cascade(ctx, username))
        {
            save_users(ctx);
            save_cars(ctx);
            save_policies(ctx);
            save_claims(ctx);
            append_operation_log(ctx, "DELETE_USER", username, "user removed with related data");
            set_message(state, L"删除用户并级联清理完成");
        }
        else
        {
            set_message(state, L"删除用户失败");
        }
        break;
    }
    case HOME_EXIT:
        *running = 0;
        break;
    default:
        break;
    }
}

/* 管理员多一个删除用户按钮，因此两种身份使用不同按钮数组做命中检测。 */
int hit_test_home(const AppContext *ctx, int x, int y)
{
    int count;
    const Button *buttons = home_buttons(ctx, &count);
    return hit_test_buttons(buttons, count, x, y);
}
