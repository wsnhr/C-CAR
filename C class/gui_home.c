#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

void draw_home(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    int i;
    draw_title(L"好车主助手");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i) draw_button(&buttons[i]);
    draw_content_panel();
    settextstyle(28, 0, L"Segoe UI");
    settextcolor(RGB(40, 50, 70));
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 30, L"控制台功能已移至图形界面演示");
    settextstyle(20, 0, L"Segoe UI");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 90, L"1. 用户：注册、登录、登出（左侧按钮）");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 130, L"2. 车辆：添加、查看、查找、编辑、删除");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 170, L"3. 保险：保单、理赔、结案管理");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 210, L"4. 数据使用 common.h 中的 API 存取（文件持久化）");
}

static void handle_register(AppContext* ctx, GuiState* state)
{
    char username[20] = {0}, password[20] = {0};
    if (!prompt_char_field(L"注册", L"请输入用户名：", username, sizeof(username), "")) return;
    if (!validate_string_len(username, (int)sizeof(((User*)0)->username))) { set_message(state, L"用户名长度不合法（1-19 字符）"); return; }
    if (!prompt_char_field(L"注册", L"请输入密码：", password, sizeof(password), "")) return;
    if (!validate_string_len(password, (int)sizeof(((User*)0)->password))) { set_message(state, L"密码长度不合法（1-19 字符）"); return; }
    if (register_user_account(ctx, username, password)) { set_message(state, L"注册成功"); append_operation_log(ctx, "ADD_USER", username, "registered"); }
    else { if (ctx->user_count >= MAX_USERS) set_message(state, L"用户数量已达上限"); else if (strcmp(username, "admin") == 0 || user_exists(ctx, username)) set_message(state, L"注册失败：用户名已存在"); else set_message(state, L"注册失败"); }
}

static void handle_login(AppContext* ctx, GuiState* state)
{
    char username[20] = {0}, password[20] = {0};
    if (!prompt_char_field(L"登录", L"请输入用户名：", username, sizeof(username), "")) return;
    if (!validate_string_len(username, (int)sizeof(((User*)0)->username))) { set_message(state, L"用户名长度不合法"); return; }
    if (!prompt_char_field(L"登录", L"请输入密码：", password, sizeof(password), "")) return;
    if (!validate_string_len(password, (int)sizeof(((User*)0)->password))) { set_message(state, L"密码长度不合法"); return; }
    if (login_user_account(ctx, username, password)) { if (app_is_admin(ctx)) set_message(state, L"以管理员身份登录"); else set_message(state, L"登录成功"); append_operation_log(ctx, "LOGIN", username, "user login"); return; }
    set_message(state, L"登录失败：用户名或密码不匹配");
}

static void handle_logout(AppContext* ctx, GuiState* state) { append_operation_log(ctx, "LOGOUT", ctx->current_user, "user logout"); logout_current_user(ctx); set_message(state, L"已登出"); }

void handle_home_action(AppContext* ctx, GuiState* state, int action, int* running)
{
    switch (action) {
    case HOME_REGISTER: handle_register(ctx, state); break;
    case HOME_LOGIN: handle_login(ctx, state); break;
    case HOME_LOGOUT: handle_logout(ctx, state); break;
    case HOME_CARS: state->screen = SCREEN_CARS; state->car_show_mine = 0; state->car_page = 0; set_message(state, L"已进入车辆管理"); break;
    case HOME_INSURANCE: update_policy_active_status(ctx); state->screen = SCREEN_INSURANCE; state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0; set_message(state, L"已进入保单管理"); break;
    case HOME_DELETE_USER: {
        if (!is_admin(ctx)) { set_message(state, L"只有管理员可删除用户"); break; }
        char username[20] = {0}; if (!prompt_char_field(L"删除用户", L"请输入要删除的用户名：", username, sizeof(username), "")) break; if (!user_exists(ctx, username)) { set_message(state, L"未找到该用户"); break; } cascade_remove_for_user(ctx, username); save_users(ctx); save_cars(ctx); save_policies(ctx); save_claims(ctx); set_message(state, L"删除用户并级联清理完成"); break; }
    case HOME_EXIT: *running = 0; break;
    default: break;
    }
}

int hit_test_home(const AppContext* ctx, int x, int y)
{
    if (is_admin(ctx)) {
        Button buttons[] = {
            {{20, 120, 200, 165}, L"", HOME_REGISTER},
            {{20, 180, 200, 225}, L"", HOME_LOGIN},
            {{20, 240, 200, 285}, L"", HOME_LOGOUT},
            {{20, 300, 200, 345}, L"", HOME_CARS},
            {{20, 360, 200, 405}, L"", HOME_INSURANCE},
            {{20, 420, 200, 465}, L"", HOME_DELETE_USER},
            {{20, 480, 200, 525}, L"", HOME_EXIT}
        };
        return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y);
    } else {
        Button buttons[] = {
            {{20, 120, 200, 165}, L"", HOME_REGISTER},
            {{20, 180, 200, 225}, L"", HOME_LOGIN},
            {{20, 240, 200, 285}, L"", HOME_LOGOUT},
            {{20, 300, 200, 345}, L"", HOME_CARS},
            {{20, 360, 200, 405}, L"", HOME_INSURANCE},
            {{20, 420, 200, 465}, L"", HOME_EXIT}
        };
        return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y);
    }
}
