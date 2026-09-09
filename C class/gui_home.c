#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui_home.c —— 首页绘制、注册登录和页面导航
 * draw_home 只负责显示；handle_home_action 负责响应操作；hit_test_home
 * 只负责判断鼠标落在哪个按钮。三者分离后职责更清楚。
 */

static const Button HOME_ADMIN_BUTTONS[] = {{{20, 120, 200, 165}, L"注册", HOME_REGISTER},
                                            {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
                                            {{20, 240, 200, 285}, L"登出", HOME_LOGOUT},
                                            {{20, 300, 200, 345}, L"车辆管理", HOME_CARS},
                                            {{20, 360, 200, 405}, L"保单管理", HOME_INSURANCE},
                                            {{20, 420, 200, 465}, L"删除用户", HOME_DELETE_USER},
                                            {{20, 480, 200, 525}, L"退出", HOME_EXIT}};

static const Button HOME_USER_BUTTONS[] = {{{20, 120, 200, 165}, L"注册", HOME_REGISTER},
                                           {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
                                           {{20, 240, 200, 285}, L"登出", HOME_LOGOUT},
                                           {{20, 300, 200, 345}, L"车辆管理", HOME_CARS},
                                           {{20, 360, 200, 405}, L"保单管理", HOME_INSURANCE},
                                           {{20, 420, 200, 465}, L"退出", HOME_EXIT}};

/* 绘制和命中检测都通过这里取得同一份按钮定义，避免坐标不一致。 */
static const Button *home_buttons(const AppContext *ctx, int *count)
{
    if (app_is_admin(ctx))
    {
        *count = (int)(sizeof(HOME_ADMIN_BUTTONS) / sizeof(HOME_ADMIN_BUTTONS[0]));
        return HOME_ADMIN_BUTTONS;
    }
    *count = (int)(sizeof(HOME_USER_BUTTONS) / sizeof(HOME_USER_BUTTONS[0]));
    return HOME_USER_BUTTONS;
}

/* 绘制标题、状态、按钮和首页说明；const 参数保证绘制过程不改业务数据。 */
void draw_home(const AppContext *ctx, const GuiState *state)
{
    int i;
    int count;
    const Button *buttons = home_buttons(ctx, &count);
    draw_title(L"好车主助手");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i)
        draw_button(&buttons[i]);
    draw_content_panel();
    settextstyle(28, 0, L"Segoe UI");
    settextcolor(RGB(40, 50, 70));
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 30, L"控制台功能已移至图形界面演示");
    settextstyle(20, 0, L"Segoe UI");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 90, L"1. 用户：注册、登录、登出（左侧按钮）");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 130, L"2. 车辆：添加、查看、查找、编辑、删除");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 170, L"3. 保险：保单、理赔、结案管理");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 210,
              L"4. 数据由业务模块处理，并通过 storage 接口持久化");
}

/* 弹出输入框、验证长度，再调用 user.c 的统一注册接口。 */
static void handle_register(AppContext *ctx, GuiState *state)
{
    /* {0} 把数组全部初始化为 0，因此即使提前返回也始终是合法空字符串。 */
    char username[20] = {0}, password[20] = {0};
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
    if (register_user_account(ctx, username, password))
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

/* 收集用户名和密码并更新 current_user；密码不会显示在状态消息中。 */
static void handle_login(AppContext *ctx, GuiState *state)
{
    char username[20] = {0}, password[20] = {0};
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
    if (login_user_account(ctx, username, password))
    {
        if (app_is_admin(ctx))
            set_message(state, L"以管理员身份登录");
        else
            set_message(state, L"登录成功");
        append_operation_log(ctx, "LOGIN", username, "user login");
        return;
    }
    set_message(state, L"登录失败：用户名或密码不匹配");
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
    case HOME_LOGOUT:
        handle_logout(ctx, state);
        break;
    case HOME_CARS:
        state->screen = SCREEN_CARS;
        state->car_show_mine = 0;
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
