#define _CRT_SECURE_NO_WARNINGS
#include <graphics.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "common.h"
#include "gui.h"

namespace {

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 700;
const int CONTENT_LEFT = 240;
const int CONTENT_RIGHT = 970;
const int CONTENT_TOP = 110;
const int CONTENT_BOTTOM = 660;
const int ROW_HEIGHT = 34;
const int PAGE_SIZE = 12;

struct Button {
    RECT rect;
    const wchar_t* label;
    int id;
};

enum Screen { SCREEN_HOME, SCREEN_CARS, SCREEN_INSURANCE };
enum HomeAction { HOME_REGISTER = 1, HOME_LOGIN, HOME_LOGOUT, HOME_CARS, HOME_INSURANCE, HOME_EXIT };
enum CarAction { CAR_ADD = 101, CAR_LIST_ALL, CAR_LIST_MINE, CAR_FIND, CAR_MODIFY, CAR_DELETE, CAR_PREV, CAR_NEXT, CAR_BACK };
enum InsuranceAction { INS_ADD_POLICY = 201, INS_DELETE_POLICY, INS_FIND_POLICY, INS_LIST_POLICIES, INS_ADD_CLAIM, INS_LIST_CLAIMS, INS_SETTLE_CLAIM, INS_PREV, INS_NEXT, INS_BACK };
enum InsuranceView { VIEW_POLICY_LIST, VIEW_CLAIM_LIST };

struct GuiState {
    Screen screen;
    int car_page;
    int policy_page;
    int claim_page;
    bool car_show_mine;
    InsuranceView insurance_view;
    wchar_t message[512];
};

void set_message(GuiState* state, const wchar_t* text)
{
    wcsncpy_s(state->message, text ? text : L"", _TRUNCATE);
}

void char_to_wchar(const char* src, wchar_t* dest, int dest_count)
{
    if (!dest || dest_count <= 0) return;
    if (!src) {
        dest[0] = L'\0';
        return;
    }
    MultiByteToWideChar(CP_ACP, 0, src, -1, dest, dest_count);
    dest[dest_count - 1] = L'\0';
}

void wchar_to_char(const wchar_t* src, char* dest, int dest_count)
{
    if (!dest || dest_count <= 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    WideCharToMultiByte(CP_ACP, 0, src, -1, dest, dest_count, NULL, NULL);
    dest[dest_count - 1] = '\0';
}

bool prompt_text(const wchar_t* title, const wchar_t* prompt, wchar_t* buffer, int count, const wchar_t* default_value = L"")
{
    if (!buffer || count <= 0) return false;
    buffer[0] = L'\0';
    return InputBox(buffer, count, prompt, title, default_value);
}

bool prompt_char_field(const wchar_t* title, const wchar_t* prompt, char* dest, int dest_count, const char* default_value = "")
{
    wchar_t text[256] = { 0 };
    wchar_t initial[256] = { 0 };
    char_to_wchar(default_value, initial, 256);
    if (!prompt_text(title, prompt, text, 256, initial)) return false;
    if (text[0] == L'\0') return false;
    wchar_to_char(text, dest, dest_count);
    return dest[0] != '\0';
}

bool prompt_double_field(const wchar_t* title, const wchar_t* prompt, double* value, double default_value = 0.0)
{
    wchar_t text[64] = { 0 };
    wchar_t initial[64] = { 0 };
    _snwprintf_s(initial, 64, _TRUNCATE, L"%.2f", default_value);
    if (!prompt_text(title, prompt, text, 64, initial)) return false;
    if (text[0] == L'\0') return false;
    *value = _wtof(text);
    return true;
}

bool prompt_int_field(const wchar_t* title, const wchar_t* prompt, int* value, int default_value)
{
    wchar_t text[64] = { 0 };
    wchar_t initial[64] = { 0 };
    _snwprintf_s(initial, 64, _TRUNCATE, L"%d", default_value);
    if (!prompt_text(title, prompt, text, 64, initial)) return false;
    if (text[0] == L'\0') return false;
    *value = _wtoi(text);
    return true;
}

bool prompt_date_field(const wchar_t* title, Date* date, const Date* current = NULL)
{
    int year = current ? current->year : 2024;
    int month = current ? current->month : 1;
    int day = current ? current->day : 1;
    if (!prompt_int_field(title, L"Input year, e.g. 2024", &year, year)) return false;
    if (!prompt_int_field(title, L"Input month, 1-12", &month, month)) return false;
    if (!prompt_int_field(title, L"Input day, 1-31", &day, day)) return false;
    date->year = year;
    date->month = month;
    date->day = day;
    return true;
}

const wchar_t* violation_to_text(ViolationLevel level)
{
    switch (level) {
    case VIOLATION_VERY_MINOR: return L"VeryMinor";
    case VIOLATION_MINOR: return L"Minor";
    case VIOLATION_MODERATE: return L"Moderate";
    case VIOLATION_RELATIVELY_SERIOUS: return L"RelSerious";
    case VIOLATION_SERIOUS: return L"Serious";
    default: return L"None";
    }
}

bool prompt_violation_field(const wchar_t* title, ViolationLevel* level, ViolationLevel current = VIOLATION_NONE)
{
    int value = (int)current;
    if (!prompt_int_field(title, L"Input violation level: 0 None, 1 VeryMinor, 2 Minor, 3 Moderate, 4 RelSerious, 5 Serious", &value, value)) return false;
    if (value < VIOLATION_NONE || value > VIOLATION_SERIOUS) value = VIOLATION_NONE;
    *level = (ViolationLevel)value;
    return true;
}

bool is_logged_in(const AppContext* ctx)
{
    return ctx->current_user[0] != '\0';
}

bool point_in_rect(int x, int y, const RECT& rect)
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void fill_rect_color(int left, int top, int right, int bottom, COLORREF line, COLORREF fill)
{
    setlinecolor(line);
    setfillcolor(fill);
    solidrectangle(left, top, right, bottom);
}

void draw_text_rect(const wchar_t* text, RECT rect, UINT format)
{
    drawtext(text, &rect, format);
}

void draw_button(const Button& button)
{
    fill_rect_color(button.rect.left, button.rect.top, button.rect.right, button.rect.bottom, RGB(210, 220, 235), RGB(245, 248, 252));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(button.label, button.rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void draw_title(const wchar_t* title)
{
    settextcolor(RGB(25, 35, 55));
    setbkmode(TRANSPARENT);
    settextstyle(34, 0, L"Segoe UI");
    outtextxy(30, 28, title);
}

void draw_status(const AppContext* ctx)
{
    wchar_t status[128] = { 0 };
    wchar_t user[64] = { 0 };
    if (is_logged_in(ctx)) char_to_wchar(ctx->current_user, user, 64);
    _snwprintf_s(status, 128, _TRUNCATE, L"Current user: %s", is_logged_in(ctx) ? user : L"Not logged in");
    settextstyle(20, 0, L"Segoe UI");
    settextcolor(RGB(80, 90, 110));
    outtextxy(30, 72, status);
}

void draw_message_box(const GuiState* state)
{
    RECT rect = { CONTENT_LEFT, 20, CONTENT_RIGHT, 88 };
    fill_rect_color(rect.left, rect.top, rect.right, rect.bottom, RGB(220, 228, 238), RGB(251, 253, 255));
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(70, 80, 95));
    draw_text_rect(state->message, rect, DT_LEFT | DT_VCENTER | DT_WORDBREAK);
}

void draw_content_panel()
{
    fill_rect_color(CONTENT_LEFT, CONTENT_TOP, CONTENT_RIGHT, CONTENT_BOTTOM, RGB(220, 228, 238), RGB(255, 255, 255));
}

void format_car_line(const Car* car, wchar_t* line, size_t count)
{
    wchar_t plate[32], brand[64], model[64], owner[64];
    char_to_wchar(car->plate, plate, 32);
    char_to_wchar(car->brand, brand, 64);
    char_to_wchar(car->model, model, 64);
    char_to_wchar(car->owner, owner, 64);
    _snwprintf_s(line, count, _TRUNCATE, L"%-8s %-10s %-10s %-10s %-10s %04d-%02d-%02d %.2f", plate, brand, model, owner, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price);
}

void format_policy_line(const Policy* policy, wchar_t* line, size_t count)
{
    wchar_t policy_id[40], plate[32], owner[64], desc[96];
    char_to_wchar(policy->policy_id, policy_id, 40);
    char_to_wchar(policy->plate, plate, 32);
    char_to_wchar(policy->owner, owner, 64);
    char_to_wchar(policy->coverage_desc, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-8s %-10s %.2f %.2f %-8s %s", policy_id, plate, owner, policy->coverage_limit, policy->premium, policy->active ? L"Active" : L"Inactive", desc);
}

void format_claim_line(const Claim* claim, wchar_t* line, size_t count)
{
    wchar_t claim_id[40], policy_id[40], plate[32], claimant[64], desc[96];
    char_to_wchar(claim->claim_id, claim_id, 40);
    char_to_wchar(claim->policy_id, policy_id, 40);
    char_to_wchar(claim->plate, plate, 32);
    char_to_wchar(claim->claimant, claimant, 64);
    char_to_wchar(claim->description, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-10s %-8s %-10s %.2f %.2f %-8s %s", claim_id, policy_id, plate, claimant, claim->request_amount, claim->approved_amount, claim->settled ? L"Settled" : L"Open", desc);
}

int collect_visible_cars(const AppContext* ctx, bool only_mine, int* indices, int max_count)
{
    int count = 0;
    for (int i = 0; i < ctx->car_count && count < max_count; ++i) {
        if (!only_mine || strcmp(ctx->cars[i].owner, ctx->current_user) == 0) indices[count++] = i;
    }
    return count;
}

void draw_table_header(const wchar_t* text)
{
    RECT rect = { CONTENT_LEFT + 18, CONTENT_TOP + 15, CONTENT_RIGHT - 18, CONTENT_TOP + 45 };
    settextstyle(18, 0, L"Consolas");
    settextcolor(RGB(55, 65, 80));
    draw_text_rect(text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void draw_rows(const wchar_t** lines, int line_count)
{
    settextstyle(18, 0, L"Consolas");
    for (int i = 0; i < line_count; ++i) {
        int top = CONTENT_TOP + 55 + i * ROW_HEIGHT;
        fill_rect_color(CONTENT_LEFT + 12, top, CONTENT_RIGHT - 12, top + ROW_HEIGHT - 4, RGB(238, 242, 247), i % 2 == 0 ? RGB(250, 252, 255) : RGB(244, 248, 252));
        RECT rect = { CONTENT_LEFT + 22, top + 4, CONTENT_RIGHT - 20, top + ROW_HEIGHT - 4 };
        settextcolor(RGB(45, 55, 72));
        draw_text_rect(lines[i], rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

void draw_footer_page(int page, int total_items)
{
    int total_pages = total_items == 0 ? 1 : (total_items + PAGE_SIZE - 1) / PAGE_SIZE;
    wchar_t page_text[64] = { 0 };
    _snwprintf_s(page_text, 64, _TRUNCATE, L"Page %d / %d, total %d", page + 1, total_pages, total_items);
    RECT rect = { CONTENT_LEFT + 20, CONTENT_BOTTOM - 40, CONTENT_RIGHT - 150, CONTENT_BOTTOM - 10 };
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(90, 100, 120));
    draw_text_rect(page_text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void draw_home(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    draw_title(L"Vehicle Insurance System");
    draw_status(ctx);
    draw_message_box(state);
    for (int i = 0; i < count; ++i) draw_button(buttons[i]);
    draw_content_panel();
    settextstyle(28, 0, L"Segoe UI");
    settextcolor(RGB(40, 50, 70));
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 30, L"Console workflow moved into GUI");
    settextstyle(20, 0, L"Segoe UI");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 90, L"1. Register, login, logout");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 130, L"2. Car add, list, find, edit, delete");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 170, L"3. Policy, claim and settlement");
    outtextxy(CONTENT_LEFT + 30, CONTENT_TOP + 210, L"4. Data still uses common.h APIs");
}

void draw_cars(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    draw_title(L"Car Management");
    draw_status(ctx);
    draw_message_box(state);
    for (int i = 0; i < count; ++i) draw_button(buttons[i]);
    draw_content_panel();
    draw_table_header(L"Plate    Brand      Model      Owner      Violation  Purchase Date Price");
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
    for (int i = start; i < end; ++i) {
        format_car_line(&ctx->cars[indices[i]], buffer[line_count], 256);
        lines[line_count] = buffer[line_count];
        ++line_count;
    }
    if (line_count == 0) {
        wcsncpy_s(buffer[0], state->car_show_mine ? L"No cars for current user" : L"No car records", _TRUNCATE);
        lines[0] = buffer[0];
        line_count = 1;
    }
    draw_rows(lines, line_count);
    draw_footer_page(page, total);
}

void draw_insurance(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    draw_title(L"Insurance Management");
    draw_status(ctx);
    draw_message_box(state);
    for (int i = 0; i < count; ++i) draw_button(buttons[i]);
    draw_content_panel();
    if (state->insurance_view == VIEW_POLICY_LIST) {
        draw_table_header(L"Policy ID  Plate    Owner      Coverage Premium Status   Description");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int total = ctx->policy_count;
        int page = state->policy_page;
        int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page >= total_pages) page = total_pages - 1;
        if (page < 0) page = 0;
        int start = page * PAGE_SIZE;
        int end = start + PAGE_SIZE;
        if (end > total) end = total;
        int line_count = 0;
        for (int i = start; i < end; ++i) {
            format_policy_line(&ctx->policies[i], buffer[line_count], 256);
            lines[line_count] = buffer[line_count];
            ++line_count;
        }
        if (line_count == 0) {
            wcsncpy_s(buffer[0], L"No policy records", _TRUNCATE);
            lines[0] = buffer[0];
            line_count = 1;
        }
        draw_rows(lines, line_count);
        draw_footer_page(page, total);
    } else {
        draw_table_header(L"Claim ID   Policy ID  Plate    Claimant   Request Approved Status   Description");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int total = ctx->claim_count;
        int page = state->claim_page;
        int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page >= total_pages) page = total_pages - 1;
        if (page < 0) page = 0;
        int start = page * PAGE_SIZE;
        int end = start + PAGE_SIZE;
        if (end > total) end = total;
        int line_count = 0;
        for (int i = start; i < end; ++i) {
            format_claim_line(&ctx->claims[i], buffer[line_count], 256);
            lines[line_count] = buffer[line_count];
            ++line_count;
        }
        if (line_count == 0) {
            wcsncpy_s(buffer[0], L"No claim records", _TRUNCATE);
            lines[0] = buffer[0];
            line_count = 1;
        }
        draw_rows(lines, line_count);
        draw_footer_page(page, total);
    }
}

int hit_test_buttons(const Button* buttons, int count, int x, int y)
{
    for (int i = 0; i < count; ++i) {
        if (point_in_rect(x, y, buttons[i].rect)) return buttons[i].id;
    }
    return 0;
}

void handle_register(AppContext* ctx, GuiState* state)
{
    if (ctx->user_count >= MAX_USERS) {
        set_message(state, L"User limit reached");
        return;
    }
    char username[20] = { 0 };
    char password[20] = { 0 };
    if (!prompt_char_field(L"Register", L"Input username", username, sizeof(username))) return;
    if (user_exists(ctx, username)) {
        set_message(state, L"Register failed: username exists");
        return;
    }
    if (!prompt_char_field(L"Register", L"Input password", password, sizeof(password))) return;
    strcpy(ctx->users[ctx->user_count].username, username);
    strcpy(ctx->users[ctx->user_count].password, password);
    ctx->user_count++;
    save_users(ctx);
    set_message(state, L"Register success");
}

void handle_login(AppContext* ctx, GuiState* state)
{
    char username[20] = { 0 };
    char password[20] = { 0 };
    if (!prompt_char_field(L"Login", L"Input username", username, sizeof(username))) return;
    if (!prompt_char_field(L"Login", L"Input password", password, sizeof(password))) return;
    for (int i = 0; i < ctx->user_count; ++i) {
        if (strcmp(ctx->users[i].username, username) == 0 && strcmp(ctx->users[i].password, password) == 0) {
            strcpy(ctx->current_user, username);
            set_message(state, L"Login success");
            return;
        }
    }
    set_message(state, L"Login failed");
}

void handle_logout(AppContext* ctx, GuiState* state)
{
    ctx->current_user[0] = '\0';
    set_message(state, L"Logged out");
}

void handle_add_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"Login first before adding car");
        return;
    }
    char plate[10] = { 0 };
    char brand[20] = { 0 };
    char model[20] = { 0 };
    ViolationLevel violation = VIOLATION_NONE;
    Date date = { 2024, 1, 1 };
    double price = 0.0;
    if (!prompt_char_field(L"Add Car", L"Input plate", plate, sizeof(plate))) return;
    if (!prompt_char_field(L"Add Car", L"Input brand", brand, sizeof(brand))) return;
    if (!prompt_char_field(L"Add Car", L"Input model", model, sizeof(model))) return;
    if (!prompt_violation_field(L"Add Car", &violation)) return;
    if (!prompt_date_field(L"Add Car", &date)) return;
    if (!prompt_double_field(L"Add Car", L"Input purchase price", &price, 0.0)) return;
    if (add_car(ctx, plate, brand, model, ctx->current_user, violation, date, price) == 0) {
        save_cars(ctx);
        set_message(state, L"Add car success");
        state->car_show_mine = false;
    }
    else {
        set_message(state, L"Add car failed");
    }
}

void handle_find_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = { 0 };
    if (!prompt_char_field(L"Find Car", L"Input plate", plate, sizeof(plate))) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"Car not found");
        return;
    }
    wchar_t plate_t[32], brand_t[64], model_t[64], owner_t[64], message[512];
    char_to_wchar(car->plate, plate_t, 32);
    char_to_wchar(car->brand, brand_t, 64);
    char_to_wchar(car->model, model_t, 64);
    char_to_wchar(car->owner, owner_t, 64);
    _snwprintf_s(message, 512, _TRUNCATE, L"Plate:%s Brand:%s Model:%s Owner:%s Violation:%s Date:%04d-%02d-%02d Price:%.2f", plate_t, brand_t, model_t, owner_t, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price);
    set_message(state, message);
}

void handle_modify_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"Login first before editing car");
        return;
    }
    char plate[10] = { 0 };
    if (!prompt_char_field(L"Edit Car", L"Input plate to edit", plate, sizeof(plate))) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"Car not found");
        return;
    }
    if (strcmp(car->owner, ctx->current_user) != 0) {
        set_message(state, L"Only current user can edit own car");
        return;
    }
    char brand[20] = { 0 };
    char model[20] = { 0 };
    ViolationLevel violation = car->violation;
    Date date = car->purchase_date;
    double price = car->purchase_price;
    if (!prompt_char_field(L"Edit Car", L"Input new brand", brand, sizeof(brand), car->brand)) return;
    if (!prompt_char_field(L"Edit Car", L"Input new model", model, sizeof(model), car->model)) return;
    if (!prompt_violation_field(L"Edit Car", &violation, car->violation)) return;
    if (!prompt_date_field(L"Edit Car", &date, &car->purchase_date)) return;
    if (!prompt_double_field(L"Edit Car", L"Input new purchase price", &price, car->purchase_price)) return;
    if (modify_car(ctx, plate, brand, model, NULL, violation, date, price) == 0) {
        save_cars(ctx);
        set_message(state, L"Edit car success");
    }
    else {
        set_message(state, L"Edit car failed");
    }
}

void handle_delete_car(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"Login first before deleting car");
        return;
    }
    char plate[10] = { 0 };
    if (!prompt_char_field(L"Delete Car", L"Input plate to delete", plate, sizeof(plate))) return;
    Car* car = find_car(ctx, plate);
    if (!car) {
        set_message(state, L"Car not found");
        return;
    }
    if (strcmp(car->owner, ctx->current_user) != 0) {
        set_message(state, L"Only current user can delete own car");
        return;
    }
    if (remove_car(ctx, plate) == 0) {
        save_cars(ctx);
        set_message(state, L"Delete car success");
    }
    else {
        set_message(state, L"Delete car failed");
    }
}

void handle_add_policy(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"Login first before adding policy");
        return;
    }
    char policy_id[24] = { 0 };
    char plate[10] = { 0 };
    char desc[64] = { 0 };
    if (!prompt_char_field(L"Add Policy", L"Input policy id", policy_id, sizeof(policy_id))) return;
    if (!prompt_char_field(L"Add Policy", L"Input plate", plate, sizeof(plate))) return;
    if (!prompt_char_field(L"Add Policy", L"Input policy description", desc, sizeof(desc))) return;
    if (add_policy(ctx, policy_id, plate, ctx->current_user, desc) == 0) {
        set_message(state, L"Add policy success");
        state->insurance_view = VIEW_POLICY_LIST;
    }
    else {
        set_message(state, L"Add policy failed");
    }
}

void handle_delete_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = { 0 };
    if (!prompt_char_field(L"Delete Policy", L"Input policy id", policy_id, sizeof(policy_id))) return;
    if (remove_policy(ctx, policy_id) == 0) set_message(state, L"Delete policy success");
    else set_message(state, L"Delete policy failed");
}

void handle_find_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = { 0 };
    if (!prompt_char_field(L"Find Policy", L"Input policy id", policy_id, sizeof(policy_id))) return;
    Policy* policy = find_policy(ctx, policy_id);
    if (!policy) {
        set_message(state, L"Policy not found");
        return;
    }
    wchar_t id_t[40], plate_t[32], owner_t[64], desc_t[96], message[512];
    char_to_wchar(policy->policy_id, id_t, 40);
    char_to_wchar(policy->plate, plate_t, 32);
    char_to_wchar(policy->owner, owner_t, 64);
    char_to_wchar(policy->coverage_desc, desc_t, 96);
    _snwprintf_s(message, 512, _TRUNCATE, L"Policy:%s Plate:%s Owner:%s Limit:%.2f Premium:%.2f Status:%s Desc:%s", id_t, plate_t, owner_t, policy->coverage_limit, policy->premium, policy->active ? L"Active" : L"Inactive", desc_t);
    set_message(state, message);
}

void handle_add_claim(AppContext* ctx, GuiState* state)
{
    if (!is_logged_in(ctx)) {
        set_message(state, L"Login first before adding claim");
        return;
    }
    char claim_id[24] = { 0 };
    char policy_id[24] = { 0 };
    char desc[256] = { 0 };
    double amount = 0.0;
    if (!prompt_char_field(L"Add Claim", L"Input claim id", claim_id, sizeof(claim_id))) return;
    if (!prompt_char_field(L"Add Claim", L"Input policy id", policy_id, sizeof(policy_id))) return;
    if (!prompt_double_field(L"Add Claim", L"Input request amount", &amount, 1000.0)) return;
    if (!prompt_char_field(L"Add Claim", L"Input claim description", desc, sizeof(desc))) return;
    if (add_claim(ctx, claim_id, policy_id, ctx->current_user, desc, amount) == 0) {
        set_message(state, L"Add claim success");
        state->insurance_view = VIEW_CLAIM_LIST;
    }
    else {
        set_message(state, L"Add claim failed");
    }
}

void handle_settle_claim(AppContext* ctx, GuiState* state)
{
    char claim_id[24] = { 0 };
    if (!prompt_char_field(L"Settle Claim", L"Input claim id", claim_id, sizeof(claim_id))) return;
    if (settle_claim(ctx, claim_id) == 0) set_message(state, L"Settle claim success");
    else set_message(state, L"Settle claim failed");
}

void handle_home_action(AppContext* ctx, GuiState* state, int action, bool* running)
{
    switch (action) {
    case HOME_REGISTER: handle_register(ctx, state); break;
    case HOME_LOGIN: handle_login(ctx, state); break;
    case HOME_LOGOUT: handle_logout(ctx, state); break;
    case HOME_CARS: state->screen = SCREEN_CARS; state->car_show_mine = false; state->car_page = 0; set_message(state, L"Entered car management"); break;
    case HOME_INSURANCE: state->screen = SCREEN_INSURANCE; state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0; set_message(state, L"Entered insurance management"); break;
    case HOME_EXIT: *running = false; break;
    default: break;
    }
}

void handle_car_action(AppContext* ctx, GuiState* state, int action)
{
    int indices[MAX_CARS] = { 0 };
    int total = collect_visible_cars(ctx, state->car_show_mine, indices, MAX_CARS);
    int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    switch (action) {
    case CAR_ADD: handle_add_car(ctx, state); break;
    case CAR_LIST_ALL: state->car_show_mine = false; state->car_page = 0; set_message(state, L"Showing all cars"); break;
    case CAR_LIST_MINE: state->car_show_mine = true; state->car_page = 0; set_message(state, L"Showing current user cars"); break;
    case CAR_FIND: handle_find_car(ctx, state); break;
    case CAR_MODIFY: handle_modify_car(ctx, state); break;
    case CAR_DELETE: handle_delete_car(ctx, state); break;
    case CAR_PREV: if (state->car_page > 0) state->car_page--; break;
    case CAR_NEXT: if (state->car_page + 1 < total_pages) state->car_page++; break;
    case CAR_BACK: state->screen = SCREEN_HOME; set_message(state, L"Back to home"); break;
    default: break;
    }
}

void handle_insurance_action(AppContext* ctx, GuiState* state, int action)
{
    int total = state->insurance_view == VIEW_POLICY_LIST ? ctx->policy_count : ctx->claim_count;
    int* page = state->insurance_view == VIEW_POLICY_LIST ? &state->policy_page : &state->claim_page;
    int total_pages = total == 0 ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    switch (action) {
    case INS_ADD_POLICY: handle_add_policy(ctx, state); break;
    case INS_DELETE_POLICY: handle_delete_policy(ctx, state); break;
    case INS_FIND_POLICY: handle_find_policy(ctx, state); break;
    case INS_LIST_POLICIES: state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0; set_message(state, L"Showing policy list"); break;
    case INS_ADD_CLAIM: handle_add_claim(ctx, state); break;
    case INS_LIST_CLAIMS: state->insurance_view = VIEW_CLAIM_LIST; state->claim_page = 0; set_message(state, L"Showing claim list"); break;
    case INS_SETTLE_CLAIM: handle_settle_claim(ctx, state); break;
    case INS_PREV: if (*page > 0) (*page)--; break;
    case INS_NEXT: if (*page + 1 < total_pages) (*page)++; break;
    case INS_BACK: state->screen = SCREEN_HOME; set_message(state, L"Back to home"); break;
    default: break;
    }
}

void draw_ui(const AppContext* ctx, const GuiState* state)
{
    setbkcolor(RGB(236, 242, 248));
    cleardevice();
    if (state->screen == SCREEN_HOME) {
        Button buttons[] = {
            {{20, 120, 200, 165}, L"Register", HOME_REGISTER},
            {{20, 180, 200, 225}, L"Login", HOME_LOGIN},
            {{20, 240, 200, 285}, L"Logout", HOME_LOGOUT},
            {{20, 300, 200, 345}, L"Cars", HOME_CARS},
            {{20, 360, 200, 405}, L"Insurance", HOME_INSURANCE},
            {{20, 420, 200, 465}, L"Exit", HOME_EXIT}
        };
        draw_home(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    } else if (state->screen == SCREEN_CARS) {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"Add Car", CAR_ADD},
            {{20, 172, 200, 212}, L"List All", CAR_LIST_ALL},
            {{20, 224, 200, 264}, L"My Cars", CAR_LIST_MINE},
            {{20, 276, 200, 316}, L"Find Car", CAR_FIND},
            {{20, 328, 200, 368}, L"Edit Car", CAR_MODIFY},
            {{20, 380, 200, 420}, L"Delete Car", CAR_DELETE},
            {{20, 432, 95, 472}, L"Prev", CAR_PREV},
            {{125, 432, 200, 472}, L"Next", CAR_NEXT},
            {{20, 492, 200, 532}, L"Home", CAR_BACK}
        };
        draw_cars(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    } else {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"Add Policy", INS_ADD_POLICY},
            {{20, 172, 200, 212}, L"Del Policy", INS_DELETE_POLICY},
            {{20, 224, 200, 264}, L"Find Policy", INS_FIND_POLICY},
            {{20, 276, 200, 316}, L"List Policy", INS_LIST_POLICIES},
            {{20, 328, 200, 368}, L"Add Claim", INS_ADD_CLAIM},
            {{20, 380, 200, 420}, L"List Claims", INS_LIST_CLAIMS},
            {{20, 432, 200, 472}, L"Settle", INS_SETTLE_CLAIM},
            {{20, 484, 95, 524}, L"Prev", INS_PREV},
            {{125, 484, 200, 524}, L"Next", INS_NEXT},
            {{20, 544, 200, 584}, L"Home", INS_BACK}
        };
        draw_insurance(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    }
    FlushBatchDraw();
}

int hit_test_home(int x, int y)
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

int hit_test_cars(int x, int y)
{
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
}

int hit_test_insurance(int x, int y)
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

} // namespace

extern "C" void gui_run(AppContext* ctx)
{
    load_users(ctx);
    load_cars(ctx);
    GuiState state = {};
    state.screen = SCREEN_HOME;
    state.insurance_view = VIEW_POLICY_LIST;
    set_message(&state, L"Choose a function from the left buttons");
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, EX_SHOWCONSOLE);
    BeginBatchDraw();
    bool running = true;
    while (running) {
        draw_ui(ctx, &state);
        ExMessage msg;
        if (peekmessage(&msg, EM_MOUSE, true) && msg.message == WM_LBUTTONDOWN) {
            int action = 0;
            if (state.screen == SCREEN_HOME) action = hit_test_home(msg.x, msg.y);
            else if (state.screen == SCREEN_CARS) action = hit_test_cars(msg.x, msg.y);
            else action = hit_test_insurance(msg.x, msg.y);
            if (state.screen == SCREEN_HOME) handle_home_action(ctx, &state, action, &running);
            else if (state.screen == SCREEN_CARS) handle_car_action(ctx, &state, action);
            else handle_insurance_action(ctx, &state, action);
        }
    }
    EndBatchDraw();
    closegraph();
}
