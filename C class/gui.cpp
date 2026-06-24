#define _CRT_SECURE_NO_WARNINGS
#include <graphics.h>
#include <windows.h>
#include <wchar.h>
#include <string.h>
#include "common.h"
#include "gui.h"

/*
    本文件：图形界面（C 风格，基于 EasyX）
    目的：尽量使用标准 C 语法实现中文界面，保留原有逻辑并增加注释以便维护。
    说明：使用 EasyX 库（graphics.h）绘制窗口和控件；窗口内部交互基于 peekmessage/EM_MOUSE。
    注意：编译前请确保项目已正确配置 EasyX 的 include 与 lib 路径。
*/

/* 常量定义（C 风格） */
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 700
#define CONTENT_LEFT 240
#define CONTENT_RIGHT 970
#define CONTENT_TOP 110
#define CONTENT_BOTTOM 660
#define ROW_HEIGHT 34
#define PAGE_SIZE 12

/* 按钮结构：使用 RECT 定义区域，label 为宽字符中文文本 */
typedef struct Button {
    RECT rect;
    const wchar_t* label;
    int id;
} Button;

/* 屏幕/动作枚举：定义界面状态与按键动作常量，便于维护 */
typedef enum { SCREEN_HOME, SCREEN_CARS, SCREEN_INSURANCE } Screen;
typedef enum { HOME_REGISTER = 1, HOME_LOGIN, HOME_LOGOUT, HOME_CARS, HOME_INSURANCE, HOME_EXIT } HomeAction;
typedef enum { CAR_ADD = 101, CAR_LIST_ALL, CAR_LIST_MINE, CAR_FIND, CAR_MODIFY, CAR_DELETE, CAR_PREV, CAR_NEXT, CAR_BACK } CarAction;
typedef enum { INS_ADD_POLICY = 201, INS_DELETE_POLICY, INS_FIND_POLICY, INS_LIST_POLICIES, INS_ADD_CLAIM, INS_LIST_CLAIMS, INS_SETTLE_CLAIM, INS_PREV, INS_NEXT, INS_BACK } InsuranceAction;
typedef enum { VIEW_POLICY_LIST, VIEW_CLAIM_LIST } InsuranceView;

/* 界面状态结构，保存当前屏幕、分页、视图选项和提示消息 */
typedef struct GuiState {
    Screen screen;
    int car_page;
    int policy_page;
    int claim_page;
    int car_show_mine; /* use int as bool (0/1) to be C99-friendly) */
    InsuranceView insurance_view;
    wchar_t message[512];
} GuiState;

/* set_message：设置状态栏消息（宽字符） */
static void set_message(GuiState* state, const wchar_t* text)
{
    if (!state) return;
    if (text) {
        wcsncpy_s(state->message, 512, text, _TRUNCATE);
    } else {
        state->message[0] = L'\0';
    }
}

/* 字符编码辅助：char -> wchar */
static void char_to_wchar(const char* src, wchar_t* dest, int dest_count)
{
    if (!dest || dest_count <= 0) return;
    if (!src) {
        dest[0] = L'\0';
        return;
    }
    MultiByteToWideChar(CP_ACP, 0, src, -1, dest, dest_count);
    dest[dest_count - 1] = L'\0';
}

/* wchar -> char */
static void wchar_to_char(const wchar_t* src, char* dest, int dest_count)
{
    if (!dest || dest_count <= 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    WideCharToMultiByte(CP_ACP, 0, src, -1, dest, dest_count, NULL, NULL);
    dest[dest_count - 1] = '\0';
}

/* prompt_text：弹出输入对话框（EasyX 的 InputBox）
   title/prompt 为宽字符中文，buffer 返回用户输入 */
static int prompt_text(const wchar_t* title, const wchar_t* prompt, wchar_t* buffer, int count, const wchar_t* default_value)
{
    if (!buffer || count <= 0) return 0;
    buffer[0] = L'\0';
    /* InputBox 为 EasyX 提供的对话框函数，返回非0表示用户确认 */
    return InputBox(buffer, count, prompt, title, default_value ? default_value : L"");
}

/* prompt_char_field：用于获取单行文本并转换为 char 字符串（方便与现有 API 交互） */
static int prompt_char_field(const wchar_t* title, const wchar_t* prompt, char* dest, int dest_count, const char* default_value)
{
    wchar_t text[256] = { 0 };
    wchar_t initial[256] = { 0 };
    if (default_value) char_to_wchar(default_value, initial, 256);
    if (!prompt_text(title, prompt, text, 256, initial)) return 0;
    if (text[0] == L'\0') return 0;
    wchar_to_char(text, dest, dest_count);
    return dest[0] != '\0';
}

/* prompt_double_field：获取浮点数输入 */
static int prompt_double_field(const wchar_t* title, const wchar_t* prompt, double* value, double default_value)
{
    wchar_t text[64] = { 0 };
    wchar_t initial[64] = { 0 };
    _snwprintf_s(initial, 64, _TRUNCATE, L"%.2f", default_value);
    if (!prompt_text(title, prompt, text, 64, initial)) return 0;
    if (text[0] == L'\0') return 0;
    *value = _wtof(text);
    return 1;
}

/* prompt_int_field：获取整数输入 */
static int prompt_int_field(const wchar_t* title, const wchar_t* prompt, int* value, int default_value)
{
    wchar_t text[64] = { 0 };
    wchar_t initial[64] = { 0 };
    _snwprintf_s(initial, 64, _TRUNCATE, L"%d", default_value);
    if (!prompt_text(title, prompt, text, 64, initial)) return 0;
    if (text[0] == L'\0') return 0;
    *value = _wtoi(text);
    return 1;
}

/* prompt_date_field：获取日期字段（年/月/日），以三个对话框顺序输入 */
static int prompt_date_field(const wchar_t* title, Date* date, const Date* current)
{
    int year = current ? current->year : 2024;
    int month = current ? current->month : 1;
    int day = current ? current->day : 1;
    if (!prompt_int_field(title, L"请输入年份（例如：2024）", &year, year)) return 0;
    if (!prompt_int_field(title, L"请输入月份（1-12）", &month, month)) return 0;
    if (!prompt_int_field(title, L"请输入日期（1-31）", &day, day)) return 0;
    date->year = year;
    date->month = month;
    date->day = day;
    return 1;
}

/* 将违章等级转换为中文文本，便于在界面显示 */
static const wchar_t* violation_to_text(ViolationLevel level)
{
    switch (level) {
    case VIOLATION_VERY_MINOR: return L"较轻微";
    case VIOLATION_MINOR: return L"轻微";
    case VIOLATION_MODERATE: return L"中等";
    case VIOLATION_RELATIVELY_SERIOUS: return L"较严重";
    case VIOLATION_SERIOUS: return L"严重";
    default: return L"无";
    }
}

/* prompt_violation_field：输入违章等级（使用整数输入） */
static int prompt_violation_field(const wchar_t* title, ViolationLevel* level, ViolationLevel current)
{
    int value = (int)current;
    if (!prompt_int_field(title, L"请输入违章等级：0 无，1 较轻微，2 轻微，3 中等，4 较严重，5 严重", &value, value)) return 0;
    if (value < VIOLATION_NONE || value > VIOLATION_SERIOUS) value = VIOLATION_NONE;
    *level = (ViolationLevel)value;
    return 1;
}

/* 判断当前是否已登录 */
static int is_logged_in(const AppContext* ctx)
{
    return ctx->current_user[0] != '\0';
}

/* 判断是否为管理员 */
static int is_admin(const AppContext* ctx)
{
    return ctx->current_user[0] != '\0' && strcmp(ctx->current_user, "admin") == 0;
}

/* 点中检测工具：判断点是否在矩形内 (C 风格使用指针) */
static int point_in_rect(int x, int y, const RECT* rect)
{
    return x >= rect->left && x <= rect->right && y >= rect->top && y <= rect->bottom;
}

/* 绘制纯色矩形（带边线与填充），便于复用 */
static void fill_rect_color(int left, int top, int right, int bottom, COLORREF line, COLORREF fill)
{
    setlinecolor(line);
    setfillcolor(fill);
    solidrectangle(left, top, right, bottom);
}

/* 在给定矩形中绘制文本（多用途） */
static void draw_text_rect(const wchar_t* text, RECT rect, UINT format)
{
    drawtext(text, &rect, format);
}

/* 绘制按钮（统一样式），参数改为指针 */
static void draw_button(const Button* button)
{
    fill_rect_color(button->rect.left, button->rect.top, button->rect.right, button->rect.bottom, RGB(210, 220, 235), RGB(245, 248, 252));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(button->label, button->rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/* 绘制页面标题（较大字体） */
static void draw_title(const wchar_t* title)
{
    settextcolor(RGB(25, 35, 55));
    setbkmode(TRANSPARENT);
    settextstyle(34, 0, L"Segoe UI");
    outtextxy(30, 28, title);
}

/* 绘制状态栏：显示当前登录用户或未登录信息 */
static void draw_status(const AppContext* ctx)
{
    wchar_t status[128] = { 0 };
    wchar_t user[64] = { 0 };
    if (is_logged_in(ctx)) char_to_wchar(ctx->current_user, user, 64);
    if (is_logged_in(ctx)) _snwprintf_s(status, 128, _TRUNCATE, L"当前用户：%s", user);
    else wcsncpy_s(status, 128, L"当前用户：未登录", _TRUNCATE);
    settextstyle(20, 0, L"Segoe UI");
    settextcolor(RGB(80, 90, 110));
    outtextxy(30, 72, status);
}

/* 绘制消息区域（位于内容区域上方），用于显示提示或操作结果 */
static void draw_message_box(const GuiState* state)
{
    RECT rect = { CONTENT_LEFT, 20, CONTENT_RIGHT, 88 };
    fill_rect_color(rect.left, rect.top, rect.right, rect.bottom, RGB(220, 228, 238), RGB(251, 253, 255));
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(70, 80, 95));
    draw_text_rect(state->message, rect, DT_LEFT | DT_VCENTER | DT_WORDBREAK);
}

/* 绘制内容面板背景 */
static void draw_content_panel(void)
{
    fill_rect_color(CONTENT_LEFT, CONTENT_TOP, CONTENT_RIGHT, CONTENT_BOTTOM, RGB(220, 228, 238), RGB(255, 255, 255));
}

/* 格式化车辆行文字（宽字符），便于绘制表格行 */
static void format_car_line(const Car* car, wchar_t* line, size_t count)
{
    wchar_t plate[32], brand[64], model[64], owner[64];
    char_to_wchar(car->plate, plate, 32);
    char_to_wchar(car->brand, brand, 64);
    char_to_wchar(car->model, model, 64);
    char_to_wchar(car->owner, owner, 64);
    _snwprintf_s(line, count, _TRUNCATE, L"%-8s %-10s %-10s %-10s %-8s %04d-%02d-%02d %.2f", plate, brand, model, owner, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price);
}

/* 格式化保单行 */
static void format_policy_line(const Policy* policy, wchar_t* line, size_t count)
{
    wchar_t policy_id[40], plate[32], owner[64], desc[96];
    char_to_wchar(policy->policy_id, policy_id, 40);
    char_to_wchar(policy->plate, plate, 32);
    char_to_wchar(policy->owner, owner, 64);
    char_to_wchar(policy->coverage_desc, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-8s %-10s %.2f %.2f %-8s %s", policy_id, plate, owner, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"失效", desc);
}

/* 格式化理赔行 */
static void format_claim_line(const Claim* claim, wchar_t* line, size_t count)
{
    wchar_t claim_id[40], policy_id[40], plate[32], claimant[64], desc[96];
    char_to_wchar(claim->claim_id, claim_id, 40);
    char_to_wchar(claim->policy_id, policy_id, 40);
    char_to_wchar(claim->plate, plate, 32);
    char_to_wchar(claim->claimant, claimant, 64);
    char_to_wchar(claim->description, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-10s %-8s %-10s %.2f %.2f %-8s %s", claim_id, policy_id, plate, claimant, claim->request_amount, claim->approved_amount, claim->settled ? L"已结" : L"未结", desc);
}

/* 收集当前可见的车辆索引（可选仅收集属于当前用户的车辆） */
static int collect_visible_cars(const AppContext* ctx, int only_mine, int* indices, int max_count)
{
    int count = 0;
    int i;
    /* 非管理员用户强制只显示自己的车辆 */
    if (!is_admin(ctx)) only_mine = 1;
    for (i = 0; i < ctx->car_count && count < max_count; ++i) {
        if (!only_mine || strcmp(ctx->cars[i].owner, ctx->current_user) == 0) indices[count++] = i;
    }
    return count;
}

/* 绘制表头文本（固定样式） */
static void draw_table_header(const wchar_t* text)
{
    RECT rect = { CONTENT_LEFT + 18, CONTENT_TOP + 15, CONTENT_RIGHT - 18, CONTENT_TOP + 45 };
    settextstyle(18, 0, L"Consolas");
    settextcolor(RGB(55, 65, 80));
    draw_text_rect(text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 按行绘制数据（交替背景色），lines 为宽字符串数组 */
static void draw_rows(const wchar_t** lines, int line_count)
{
    int i;
    settextstyle(18, 0, L"Consolas");
    for (i = 0; i < line_count; ++i) {
        int top = CONTENT_TOP + 55 + i * ROW_HEIGHT;
        fill_rect_color(CONTENT_LEFT + 12, top, CONTENT_RIGHT - 12, top + ROW_HEIGHT - 4, RGB(238, 242, 247), i % 2 == 0 ? RGB(250, 252, 255) : RGB(244, 248, 252));
        RECT rect = { CONTENT_LEFT + 22, top + 4, CONTENT_RIGHT - 20, top + ROW_HEIGHT - 4 };
        settextcolor(RGB(45, 55, 72));
        draw_text_rect(lines[i], rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

/* 绘制分页信息（页码与总条数） */
static void draw_footer_page(int page, int total_items)
{
    int total_pages = total_items == 0 ? 1 : (total_items + PAGE_SIZE - 1) / PAGE_SIZE;
    wchar_t page_text[64] = { 0 };
    _snwprintf_s(page_text, 64, _TRUNCATE, L"第 %d / %d 页，共 %d 条", page + 1, total_pages, total_items);
    RECT rect = { CONTENT_LEFT + 20, CONTENT_BOTTOM - 40, CONTENT_RIGHT - 150, CONTENT_BOTTOM - 10 };
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(90, 100, 120));
    draw_text_rect(page_text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* 绘制首页内容（帮助说明） */
static void draw_home(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
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

/* 绘制车辆管理界面 */
static void draw_cars(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    int i;
    draw_title(L"车辆管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i) draw_button(&buttons[i]);
    draw_content_panel();
    draw_table_header(L"车牌    品牌        型号        车主        违章    购入日期     价格");
    int indices[MAX_CARS] = { 0 };
    int total = collect_visible_cars(ctx, state->car_show_mine, indices, MAX_CARS);
    int page = state->car_page;
    int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page >= total_pages) page = total_pages - 1;
    if (page < 0) page = 0;
    const wchar_t* lines[PAGE_SIZE] = { 0 };
    wchar_t buffer[PAGE_SIZE][256] = { 0 };
    int line_count = 0;
    int start = page * PAGE_SIZE;
    int end = start + PAGE_SIZE;
    if (end > total) end = total;
    for (i = start; i < end; ++i) {
        format_car_line(&ctx->cars[indices[i]], buffer[line_count], 256);
        lines[line_count] = buffer[line_count];
        ++line_count;
    }
    if (line_count == 0) {
        wcsncpy_s(buffer[0], 256, L"无车辆记录", _TRUNCATE);
        lines[0] = buffer[0];
        line_count = 1;
    }
    draw_rows(lines, line_count);
    draw_footer_page(page, total);
}

/* 绘制保险管理界面（保单或理赔视图） */
static void draw_insurance(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    int i;
    draw_title(L"保单与理赔管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i) draw_button(&buttons[i]);
    draw_content_panel();
    if (state->insurance_view == VIEW_POLICY_LIST) {
        draw_table_header(L"保单号    车牌    投保人      保额     保费   状态   描述");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int total = 0;
        int page = state->policy_page;
        int iidx = 0;
        int line_count = 0;
        /* 非管理员用户只看自己的保单 */
        for (i = 0; i < ctx->policy_count; ++i) {
            if (!is_admin(ctx) && strcmp(ctx->policies[i].owner, ctx->current_user) != 0) continue;
            if (line_count < PAGE_SIZE) {
                format_policy_line(&ctx->policies[i], buffer[line_count], 256);
                lines[line_count] = buffer[line_count];
            }
            line_count++;
        }
        total = line_count;
        /* pagination: rebuild page view */
        if (total == 0) {
            wcsncpy_s(buffer[0], 256, L"无保单记录", _TRUNCATE);
            lines[0] = buffer[0];
            draw_rows(lines, 1);
            draw_footer_page(0, 0);
            return;
        }
        int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page >= total_pages) page = total_pages - 1;
        if (page < 0) page = 0;
        int start = page * PAGE_SIZE;
        int end = start + PAGE_SIZE;
        if (end > total) end = total;
        /* prepare lines for current page */
        int idx = 0;
        for (i = 0; i < ctx->policy_count && idx < end; ++i) {
            if (!is_admin(ctx) && strcmp(ctx->policies[i].owner, ctx->current_user) != 0) continue;
            if (idx >= start && idx < end) {
                format_policy_line(&ctx->policies[i], buffer[idx - start], 256);
                lines[idx - start] = buffer[idx - start];
            }
            idx++;
        }
        draw_rows(lines, end - start);
        draw_footer_page(page, total);
    } else {
        draw_table_header(L"理赔号    保单号    车牌    申请人    申请(元)  批准(元)  状态   描述");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int total = 0;
        int page = state->claim_page;
        int line_count = 0;
        for (i = 0; i < ctx->claim_count; ++i) {
            if (!is_admin(ctx) && strcmp(ctx->claims[i].claimant, ctx->current_user) != 0) continue;
            if (line_count < PAGE_SIZE) {
                format_claim_line(&ctx->claims[i], buffer[line_count], 256);
                lines[line_count] = buffer[line_count];
            }
            line_count++;
        }
        total = line_count;
        if (total == 0) {
            wcsncpy_s(buffer[0], 256, L"无理赔记录", _TRUNCATE);
            lines[0] = buffer[0];
            draw_rows(lines, 1);
            draw_footer_page(0, 0);
            return;
        }
        int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page >= total_pages) page = total_pages - 1;
        if (page < 0) page = 0;
        int start = page * PAGE_SIZE;
        int end = start + PAGE_SIZE;
        if (end > total) end = total;
        int idx = 0;
        for (i = 0; i < ctx->claim_count && idx < end; ++i) {
            if (!is_admin(ctx) && strcmp(ctx->claims[i].claimant, ctx->current_user) != 0) continue;
            if (idx >= start && idx < end) {
                format_claim_line(&ctx->claims[i], buffer[idx - start], 256);
                lines[idx - start] = buffer[idx - start];
            }
            idx++;
        }
        draw_rows(lines, end - start);
        draw_footer_page(page, total);
    }
}

/* 命中测试：判断点击是否落在左侧按钮上，返回按钮 id */
static int hit_test_buttons(const Button* buttons, int count, int x, int y)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (point_in_rect(x, y, &buttons[i].rect)) return buttons[i].id;
    }
    return 0;
}

/* 以下一组函数实现了各类操作处理（注册、登录、车辆操作、保单/理赔操作） */

static void handle_register(AppContext* ctx, GuiState* state)
{
    if (ctx->user_count >= MAX_USERS) {
        set_message(state, L"用户数量已达上限");
        return;
    }
    char username[20] = { 0 };
    char password[20] = { 0 };
    if (!prompt_char_field(L"注册", L"请输入用户名：", username, sizeof(username), "")) return;
    if (user_exists(ctx, username)) {
        set_message(state, L"注册失败：用户名已存在");
        return;
    }
    if (!prompt_char_field(L"注册", L"请输入密码：", password, sizeof(password), "")) return;
    strcpy(ctx->users[ctx->user_count].username, username);
    strcpy(ctx->users[ctx->user_count].password, password);
    ctx->user_count++;
    save_users(ctx);
    set_message(state, L"注册成功");
}

static void handle_login(AppContext* ctx, GuiState* state)
{
    char username[20] = { 0 };
    char password[20] = { 0 };
    if (!prompt_char_field(L"登录", L"请输入用户名：", username, sizeof(username), "")) return;
    if (!prompt_char_field(L"登录", L"请输入密码：", password, sizeof(password), "")) return;
    /* 管理员内置账号 */
    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0) {
        strcpy(ctx->current_user, "admin");
        set_message(state, L"以管理员身份登录");
        return;
    }
    int i;
    for (i = 0; i < ctx->user_count; ++i) {
        if (strcmp(ctx->users[i].username, username) == 0 && strcmp(ctx->users[i].password, password) == 0) {
            strcpy(ctx->current_user, username);
            set_message(state, L"登录成功");
            return;
        }
    }
    set_message(state, L"登录失败：用户名或密码不匹配");
}

static void handle_logout(AppContext* ctx, GuiState* state)
{
    ctx->current_user[0] = '\0';
    set_message(state, L"已登出");
}

static void handle_add_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"请先登录后再添加车辆");
        return;
    }
    char plate[10] = { 0 };
    char brand[20] = { 0 };
    char model[20] = { 0 };
    char owner[20] = { 0 };
    ViolationLevel violation = VIOLATION_NONE;
    Date date = { 2024, 1, 1 };
    double price = 0.0;
    if (!prompt_char_field(L"添加车辆", L"请输入车牌：", plate, sizeof(plate), "")) return;
    if (!prompt_char_field(L"添加车辆", L"请输入品牌：", brand, sizeof(brand), "")) return;
    if (!prompt_char_field(L"添加车辆", L"请输入型号：", model, sizeof(model), "")) return;
    if (!prompt_violation_field(L"添加车辆", &violation, VIOLATION_NONE)) return;
    if (!prompt_date_field(L"添加车辆", &date, NULL)) return;
    if (!prompt_double_field(L"添加车辆", L"请输入购入价格（元）：", &price, 0.0)) return;
    if (is_admin(ctx)) {
        if (!prompt_char_field(L"添加车辆", L"请输入车主用户名：", owner, sizeof(owner), "")) return;
        if (!user_exists(ctx, owner)) {
            set_message(state, L"指定的车主不存在");
            return;
        }
    } else {
        strncpy(owner, ctx->current_user, sizeof(owner) - 1);
        owner[sizeof(owner) - 1] = '\0';
    }
    if (add_car(ctx, plate, brand, model, owner, violation, date, price) == 0) {
        save_cars(ctx);
        set_message(state, L"添加车辆成功");
        state->car_show_mine = 0;
    }
    else {
        set_message(state, L"添加车辆失败（可能车牌已存在或已达上限）");
    }
}

static void handle_find_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = { 0 };
    if (!prompt_char_field(L"查找车辆", L"请输入车牌：", plate, sizeof(plate), "")) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"未找到该车辆");
        return;
    }
    /* 如果不是管理员且该车不是当前用户的，禁止查看详细信息（仅显示未找到） */
    if (!is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0) {
        set_message(state, L"未找到该车辆");
        return;
    }
    wchar_t plate_t[32], brand_t[64], model_t[64], owner_t[64], message[512];
    char_to_wchar(car->plate, plate_t, 32);
    char_to_wchar(car->brand, brand_t, 64);
    char_to_wchar(car->model, model_t, 64);
    char_to_wchar(car->owner, owner_t, 64);
    _snwprintf_s(message, 512, _TRUNCATE, L"车牌:%s 品牌:%s 型号:%s 车主:%s 违章:%s 购入:%04d-%02d-%02d 价格:%.2f元", plate_t, brand_t, model_t, owner_t, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price);
    set_message(state, message);
}

static void handle_modify_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"请先登录后再编辑车辆");
        return;
    }
    char plate[10] = { 0 };
    if (!prompt_char_field(L"编辑车辆", L"请输入要编辑的车牌：", plate, sizeof(plate), "")) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"未找到该车辆");
        return;
    }
    if (!is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0) {
        set_message(state, L"只能编辑当前用户拥有的车辆");
        return;
    }
    char brand[20] = { 0 };
    char model[20] = { 0 };
    ViolationLevel violation = car->violation;
    Date date = car->purchase_date;
    double price = car->purchase_price;
    if (!prompt_char_field(L"编辑车辆", L"请输入新品牌（留空则保持不变）：", brand, sizeof(brand), car->brand)) return;
    if (!prompt_char_field(L"编辑车辆", L"请输入新型号（留空则保持不变）：", model, sizeof(model), car->model)) return;
    if (!prompt_violation_field(L"编辑车辆", &violation, car->violation)) return;
    if (!prompt_date_field(L"编辑车辆", &date, &car->purchase_date)) return;
    if (!prompt_double_field(L"编辑车辆", L"请输入新购入价格：", &price, car->purchase_price)) return;
    if (modify_car(ctx, plate, brand, model, NULL, violation, date, price) == 0) {
        save_cars(ctx);
        set_message(state, L"编辑车辆成功");
    }
    else {
        set_message(state, L"编辑车辆失败");
    }
}

static void handle_delete_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"请先登录后再删除车辆");
        return;
    }
    char plate[10] = { 0 };
    if (!prompt_char_field(L"删除车辆", L"请输入要删除的车牌：", plate, sizeof(plate), "")) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"未找到该车辆");
        return;
    }
    if (!is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0) {
        set_message(state, L"只能删除当前用户拥有的车辆");
        return;
    }
    if (remove_car(ctx, plate) == 0) {
        save_cars(ctx);
        set_message(state, L"删除车辆成功");
    }
    else {
        set_message(state, L"删除车辆失败");
    }
}

static void handle_add_policy(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"请先登录后再添加保单");
        return;
    }
    char policy_id[24] = { 0 };
    char plate[10] = { 0 };
    char desc[64] = { 0 };
    char owner[20] = { 0 };
    if (!prompt_char_field(L"添加保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return;
    if (!prompt_char_field(L"添加保单", L"请输入投保车牌：", plate, sizeof(plate), "")) return;
    if (!prompt_char_field(L"添加保单", L"请输入保单描述：", desc, sizeof(desc), "")) return;
    if (is_admin(ctx)) {
        if (!prompt_char_field(L"添加保单", L"请输入投保人用户名：", owner, sizeof(owner), "")) return;
        if (!user_exists(ctx, owner)) { set_message(state, L"指定的用户不存在"); return; }
    } else {
        strncpy(owner, ctx->current_user, sizeof(owner)-1);
        owner[sizeof(owner)-1] = '\0';
    }
    if (add_policy(ctx, policy_id, plate, owner, desc) == 0) {
        set_message(state, L"添加保单成功");
        state->insurance_view = VIEW_POLICY_LIST;
    }
    else {
        set_message(state, L"添加保单失败");
    }
}

static void handle_delete_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = { 0 };
    if (!prompt_char_field(L"删除保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return;
    Policy* p = find_policy(ctx, policy_id);
    if (!p) { set_message(state, L"未找到该保单"); return; }
    if (!is_admin(ctx) && strcmp(p->owner, ctx->current_user) != 0) { set_message(state, L"只能删除当前用户相关的保单"); return; }
    if (remove_policy(ctx, policy_id) == 0) set_message(state, L"删除保单成功");
    else set_message(state, L"删除保单失败");
}

static void handle_find_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = { 0 };
    if (!prompt_char_field(L"查询保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return;
    Policy* policy = find_policy(ctx, policy_id);
    if (!policy) {
        set_message(state, L"未找到保单");
        return;
    }
    if (!is_admin(ctx) && strcmp(policy->owner, ctx->current_user) != 0) { set_message(state, L"未找到保单"); return; }
    wchar_t id_t[40], plate_t[32], owner_t[64], desc_t[96], message[512];
    char_to_wchar(policy->policy_id, id_t, 40);
    char_to_wchar(policy->plate, plate_t, 32);
    char_to_wchar(policy->owner, owner_t, 64);
    char_to_wchar(policy->coverage_desc, desc_t, 96);
    _snwprintf_s(message, 512, _TRUNCATE, L"保单:%s 车牌:%s 投保人:%s 保额:%.2f 保费:%.2f 状态:%s 描述:%s", id_t, plate_t, owner_t, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"失效", desc_t);
    set_message(state, message);
}

static void handle_add_claim(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"请先登录后再提交理赔");
        return;
    }
    char claim_id[24] = { 0 };
    char policy_id[24] = { 0 };
    char desc[256] = { 0 };
    double amount = 0.0;
    char claimant[20] = {0};
    if (!prompt_char_field(L"新增理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), "")) return;
    if (!prompt_char_field(L"新增理赔", L"请输入关联保单号：", policy_id, sizeof(policy_id), "")) return;
    if (!prompt_double_field(L"新增理赔", L"请输入申请金额（元）：", &amount, 1000.0)) return;
    if (!prompt_char_field(L"新增理赔", L"请输入理赔说明：", desc, sizeof(desc), "")) return;
    if (is_admin(ctx)) {
        if (!prompt_char_field(L"新增理赔", L"请输入申请人用户名：", claimant, sizeof(claimant), "")) return;
        if (!user_exists(ctx, claimant)) { set_message(state, L"指定的用户不存在"); return; }
    } else {
        strncpy(claimant, ctx->current_user, sizeof(claimant)-1);
        claimant[sizeof(claimant)-1] = '\0';
    }
    if (add_claim(ctx, claim_id, policy_id, claimant, desc, amount) == 0) {
        set_message(state, L"提交理赔成功");
        state->insurance_view = VIEW_CLAIM_LIST;
    }
    else {
        set_message(state, L"提交理赔失败");
    }
}

static void handle_settle_claim(AppContext* ctx, GuiState* state)
{
    char claim_id[24] = { 0 };
    if (!prompt_char_field(L"结案理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), "")) return;
    Claim* c = find_claim(ctx, claim_id);
    if (!c) { set_message(state, L"未找到理赔单"); return; }
    if (!is_admin(ctx) && strcmp(c->claimant, ctx->current_user) != 0) { set_message(state, L"只能结案当前用户提交的理赔"); return; }
    if (settle_claim(ctx, claim_id) == 0) set_message(state, L"理赔结案成功");
    else set_message(state, L"理赔结案失败");
}

static void handle_home_action(AppContext* ctx, GuiState* state, int action, int* running)
{
    switch (action) {
    case HOME_REGISTER: handle_register(ctx, state); break;
    case HOME_LOGIN: handle_login(ctx, state); break;
    case HOME_LOGOUT: handle_logout(ctx, state); break;
    case HOME_CARS: state->screen = SCREEN_CARS; state->car_show_mine = 0; state->car_page = 0; set_message(state, L"已进入车辆管理"); break;
    case HOME_INSURANCE: state->screen = SCREEN_INSURANCE; state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0; set_message(state, L"已进入保单管理"); break;
    case HOME_EXIT: *running = 0; break;
    default: break;
    }
}

static void handle_car_action(AppContext* ctx, GuiState* state, int action)
{
    int indices[MAX_CARS] = { 0 };
    int total = collect_visible_cars(ctx, state->car_show_mine, indices, MAX_CARS);
    int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    switch (action) {
    case CAR_ADD: handle_add_car(ctx, state); break;
    case CAR_LIST_ALL: state->car_show_mine = 0; state->car_page = 0; set_message(state, L"显示全部车辆"); break;
    case CAR_LIST_MINE: state->car_show_mine = 1; state->car_page = 0; set_message(state, L"显示我的车辆"); break;
    case CAR_FIND: handle_find_car(ctx, state); break;
    case CAR_MODIFY: handle_modify_car(ctx, state); break;
    case CAR_DELETE: handle_delete_car(ctx, state); break;
    case CAR_PREV: if (state->car_page > 0) state->car_page--; break;
    case CAR_NEXT: if (state->car_page + 1 < total_pages) state->car_page++; break;
    case CAR_BACK: state->screen = SCREEN_HOME; set_message(state, L"返回主界面"); break;
    default: break;
    }
}

static void handle_insurance_action(AppContext* ctx, GuiState* state, int action)
{
    int total = state->insurance_view == VIEW_POLICY_LIST ? ctx->policy_count : ctx->claim_count;
    int* page = state->insurance_view == VIEW_POLICY_LIST ? &state->policy_page : &state->claim_page;
    int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    switch (action) {
    case INS_ADD_POLICY: handle_add_policy(ctx, state); break;
    case INS_DELETE_POLICY: handle_delete_policy(ctx, state); break;
    case INS_FIND_POLICY: handle_find_policy(ctx, state); break;
    case INS_LIST_POLICIES: state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0; set_message(state, L"显示保单列表"); break;
    case INS_ADD_CLAIM: handle_add_claim(ctx, state); break;
    case INS_LIST_CLAIMS: state->insurance_view = VIEW_CLAIM_LIST; state->claim_page = 0; set_message(state, L"显示理赔列表"); break;
    case INS_SETTLE_CLAIM: handle_settle_claim(ctx, state); break;
    case INS_PREV: if (*page > 0) (*page)--; break;
    case INS_NEXT: if (*page + 1 < total_pages) (*page)++; break;
    case INS_BACK: state->screen = SCREEN_HOME; set_message(state, L"返回主界面"); break;
    default: break;
    }
}

static void draw_ui(const AppContext* ctx, const GuiState* state)
{
    setbkcolor(RGB(236, 242, 248));
    cleardevice();
    if (state->screen == SCREEN_HOME) {
        Button buttons[] = {
            {{20, 120, 200, 165}, L"注册", HOME_REGISTER},
            {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
            {{20, 240, 200, 285}, L"登出", HOME_LOGOUT},
            {{20, 300, 200, 345}, L"车辆管理", HOME_CARS},
            {{20, 360, 200, 405}, L"保单管理", HOME_INSURANCE},
            {{20, 420, 200, 465}, L"退出", HOME_EXIT}
        };
        draw_home(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    } else if (state->screen == SCREEN_CARS) {
        if (is_admin(ctx)) {
            Button buttons[] = {
                {{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                {{20, 172, 200, 212}, L"查看全部", CAR_LIST_ALL},
                {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                {{20, 492, 200, 532}, L"返回", CAR_BACK}
            };
            draw_cars(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        } else {
            /* 普通用户不显示“查看全部”按钮 */
            Button buttons[] = {
                {{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                {{20, 492, 200, 532}, L"返回", CAR_BACK}
            };
            draw_cars(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        }
    } else {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"添加保单", INS_ADD_POLICY},
            {{20, 172, 200, 212}, L"删除保单", INS_DELETE_POLICY},
            {{20, 224, 200, 264}, L"查询保单", INS_FIND_POLICY},
            {{20, 276, 200, 316}, L"保单列表", INS_LIST_POLICIES},
            {{20, 328, 200, 368}, L"新增理赔", INS_ADD_CLAIM},
            {{20, 380, 200, 420}, L"理赔列表", INS_LIST_CLAIMS},
            {{20, 432, 200, 472}, L"结案", INS_SETTLE_CLAIM},
            {{20, 484, 95, 524}, L"上一页", INS_PREV},
            {{125, 484, 200, 524}, L"下一页", INS_NEXT},
            {{20, 544, 200, 584}, L"返回", INS_BACK}
        };
        draw_insurance(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    }
    FlushBatchDraw();
}

static int hit_test_home(int x, int y)
{
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

static int hit_test_cars(const AppContext* ctx, int x, int y)
{
    if (is_admin(ctx)) {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"", CAR_ADD},
            {{20, 172, 200, 212}, L"", CAR_LIST_ALL},
            {{20, 224, 200, 264}, L"", CAR_LIST_MINE},
            {{20, 276, 200, 316}, L"", CAR_FIND},
            {{20, 328, 200, 368}, L"", CAR_MODIFY},
            {{20, 380, 200, 420}, L"", CAR_DELETE},
            {{20, 432, 95, 472}, L"", CAR_PREV},
            {{125, 432, 200, 472}, L"", CAR_NEXT},
            {{20, 492, 200, 532}, L"", CAR_BACK}
        };
        return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y);
    } else {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"", CAR_ADD},
            // no '查看全部' for non-admin
            {{20, 224, 200, 264}, L"", CAR_LIST_MINE},
            {{20, 276, 200, 316}, L"", CAR_FIND},
            {{20, 328, 200, 368}, L"", CAR_MODIFY},
            {{20, 380, 200, 420}, L"", CAR_DELETE},
            {{20, 432, 95, 472}, L"", CAR_PREV},
            {{125, 432, 200, 472}, L"", CAR_NEXT},
            {{20, 492, 200, 532}, L"", CAR_BACK}
        };
        return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y);
    }
}

static int hit_test_insurance(int x, int y)
{
    Button buttons[] = {
        {{20, 120, 200, 160}, L"", INS_ADD_POLICY},
        {{20, 172, 200, 212}, L"", INS_DELETE_POLICY},
        {{20, 224, 200, 264}, L"", INS_FIND_POLICY},
        {{20, 276, 200, 316}, L"", INS_LIST_POLICIES},
        {{20, 328, 200, 368}, L"", INS_ADD_CLAIM},
        {{20, 380, 200, 420}, L"", INS_LIST_CLAIMS},
        {{20, 432, 200, 472}, L"", INS_SETTLE_CLAIM},
        {{20, 484, 95, 524}, L"", INS_PREV},
        {{125, 484, 200, 524}, L"", INS_NEXT},
        {{20, 544, 200, 584}, L"", INS_BACK}
    };
    return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y);
}

/* gui_run：对外 C 接口，启动 GUI 循环 */
extern "C" void gui_run(AppContext* ctx)
{
    /* 启动前加载数据文件 */
    load_users(ctx);
    load_cars(ctx);
    GuiState state;
    memset(&state, 0, sizeof(state));
    state.screen = SCREEN_HOME;
    state.insurance_view = VIEW_POLICY_LIST;
    set_message(&state, L"请从左侧选择功能");

    /* 初始化 EasyX 窗口并开启批量绘制 */
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, EX_SHOWCONSOLE);
    BeginBatchDraw();
    int running = 1;
    while (running) {
        /* 渲染界面 */
        draw_ui(ctx, &state);
        ExMessage msg;
        /* 非阻塞式获取鼠标消息，处理左键点击 */
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN) {
            int action = 0;
            if (state.screen == SCREEN_HOME) action = hit_test_home(msg.x, msg.y);
            else if (state.screen == SCREEN_CARS) action = hit_test_cars(ctx, msg.x, msg.y);
            else action = hit_test_insurance(msg.x, msg.y);
            if (state.screen == SCREEN_HOME) handle_home_action(ctx, &state, action, &running);
            else if (state.screen == SCREEN_CARS) handle_car_action(ctx, &state, action);
            else handle_insurance_action(ctx, &state, action);
        }
    }
    EndBatchDraw();
    closegraph();
}
