#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

static void format_car_line(const Car* car, wchar_t* line, size_t count)
{
    wchar_t plate[32], brand[64], model[64], owner[64];
    char_to_wchar(car->plate, plate, 32);
    char_to_wchar(car->brand, brand, 64);
    char_to_wchar(car->model, model, 64);
    char_to_wchar(car->owner, owner, 64);
    _snwprintf_s(line, count, _TRUNCATE, L"%-8s %-10s %-10s %-10s %-8s %04d-%02d-%02d %.2f", plate, brand, model, owner, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price);
}

static int collect_visible_cars(const AppContext* ctx, int only_mine, int* indices, int max_count)
{
    return collect_visible_car_indices(ctx, only_mine, indices, max_count);
}

void draw_cars(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    int i;
    draw_title(L"车辆管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i) draw_button(&buttons[i]);
    draw_content_panel();
    /* CHANGED: 将表头“违章”改为“违章记录” */
    draw_table_header(L"车牌    品牌        型号        车主        违章记录     购入日期     价格");
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

static void handle_add_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = {0}, brand[20] = {0}, model[20] = {0}, owner[20] = {0};
    ViolationLevel violation = VIOLATION_NONE; Date date = {0}; date.year = today_local().year; date.month = 1; date.day = 1; double price = 0.0;
    if (!prompt_char_field(L"添加车辆", L"请输入车牌：", plate, sizeof(plate), "")) return;
    if (!validate_plate(plate)) { set_message(state, L"车牌格式不合法：只允许字母数字和短横，长度限制请保证不超过9字符"); return; }
    if (!prompt_char_field(L"添加车辆", L"请输入品牌：", brand, sizeof(brand), "")) return;
    if (!validate_string_len(brand, (int)sizeof(((Car*)0)->brand))) { set_message(state, L"品牌长度不合法"); return; }
    if (!prompt_char_field(L"添加车辆", L"请输入型号：", model, sizeof(model), "")) return;
    if (!validate_string_len(model, (int)sizeof(((Car*)0)->model))) { set_message(state, L"型号长度不合法"); return; }
    if (!prompt_violation_field(L"添加车辆", &violation, VIOLATION_NONE)) return;
    if (!prompt_date_field(L"添加车辆", &date, NULL)) return;
    if (!validate_date(&date)) { set_message(state, L"购买日期不合法"); return; }
    if (!prompt_double_field(L"添加车辆", L"请输入购入价格（元）：", &price, 0.0)) return;
    if (!validate_price_value(price)) { set_message(state, L"价格不合法（必须在 0 - 1e9 范围内）"); return; }
    if (is_admin(ctx)) { if (!prompt_char_field(L"添加车辆", L"请输入车主用户名：", owner, sizeof(owner), "")) return; if (!validate_string_len(owner, (int)sizeof(((User*)0)->username)) || !user_exists(ctx, owner)) { set_message(state, L"指定的车主不存在或用户名长度不合法"); return; } }
    else { strncpy(owner, ctx->current_user, sizeof(owner)-1); owner[sizeof(owner)-1] = '\0'; }
    if (add_car_for_current_user(ctx, plate, brand, model, owner, violation, date, price)) { save_cars(ctx); append_operation_log(ctx, "ADD_CAR", plate, owner); set_message(state, L"添加车辆成功"); state->car_show_mine = 0; }
    else set_message(state, L"添加车辆失败（可能车牌已存在或已达上限）");
}

static void handle_find_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = {0}; if (!prompt_char_field(L"查找车辆", L"请输入车牌：", plate, sizeof(plate), "")) return; const Car* car = find_visible_car(ctx, plate); if (!car) { set_message(state, L"未找到该车辆"); return; }
    wchar_t plate_t[32], brand_t[64], model_t[64], owner_t[64], message[512]; char_to_wchar(car->plate, plate_t, 32); char_to_wchar(car->brand, brand_t, 64); char_to_wchar(car->model, model_t, 64); char_to_wchar(car->owner, owner_t, 64); _snwprintf_s(message, 512, _TRUNCATE, L"车牌:%s 品牌:%s 型号:%s 车主:%s 违章:%s 购入:%04d-%02d-%02d 价格:%.2f元", plate_t, brand_t, model_t, owner_t, violation_to_text(car->violation), car->purchase_date.year, car->purchase_date.month, car->purchase_date.day, car->purchase_price); set_message(state, message);
}

static void handle_modify_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = {0}; if (!prompt_char_field(L"编辑车辆", L"请输入要编辑的车牌：", plate, sizeof(plate), "")) return; if (!validate_plate(plate)) { set_message(state, L"车牌格式不合法"); return; }
    Car* car = find_car(ctx, plate); if (!car) { set_message(state, L"未找到该车辆"); return; }
    char brand[20] = {0}, model[20] = {0}; ViolationLevel violation = car->violation; Date date = car->purchase_date; double price = car->purchase_price;
    if (!prompt_char_field(L"编辑车辆", L"请输入新品牌（留空则保持不变）：", brand, sizeof(brand), car->brand)) return; if (!validate_string_len(brand, (int)sizeof(((Car*)0)->brand))) { set_message(state, L"品牌长度不合法"); return; }
    if (!prompt_char_field(L"编辑车辆", L"请输入新型号（留空则保持不变）：", model, sizeof(model), car->model)) return; if (!validate_string_len(model, (int)sizeof(((Car*)0)->model))) { set_message(state, L"型号长度不合法"); return; }
    if (!prompt_violation_field(L"编辑车辆", &violation, car->violation)) return;
    if (!prompt_date_field(L"编辑车辆", &date, &car->purchase_date)) return; if (!validate_date(&date)) { set_message(state, L"购买日期不合法"); return; }
    if (!prompt_double_field(L"编辑车辆", L"请输入新购入价格：", &price, car->purchase_price)) return; if (!validate_price_value(price)) { set_message(state, L"价格不合法（必须在 0 - 1e9 范围内）"); return; }
    if (modify_car_for_current_user(ctx, plate, brand, model, violation, date, price)) { save_cars(ctx); append_operation_log(ctx, "MODIFY_CAR", plate, "modified"); set_message(state, L"编辑车辆成功"); }
    else set_message(state, L"编辑车辆失败");
}

static void handle_delete_car(AppContext* ctx, GuiState* state)
{
    char plate[10] = {0}; if (!prompt_char_field(L"删除车辆", L"请输入要删除的车牌：", plate, sizeof(plate), "")) return; if (!validate_plate(plate)) { set_message(state, L"车牌格式不合法"); return; }
    Car* car = find_car(ctx, plate); if (!car) { set_message(state, L"未找到该车辆"); return; } if (!is_admin(ctx) && strcmp(car->owner, ctx->current_user) != 0) { set_message(state, L"只能删除当前用户拥有的车辆"); return; }
    // 先级联删除相关保单/理赔
    cascade_remove_policies_and_claims_for_plate(ctx, plate);
    if (remove_car(ctx, plate) == 0) { save_cars(ctx); save_policies(ctx); save_claims(ctx); append_operation_log(ctx, "DELETE_CAR", plate, "deleted with cascade"); set_message(state, L"删除车辆成功"); }
    else set_message(state, L"删除车辆失败");
}

void handle_car_action(AppContext* ctx, GuiState* state, int action)
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

int hit_test_cars(const AppContext* ctx, int x, int y) { if (is_admin(ctx)) { Button buttons[] = { {{20, 120, 200, 160}, L"", CAR_ADD}, {{20, 172, 200, 212}, L"", CAR_LIST_ALL}, {{20, 224, 200, 264}, L"", CAR_LIST_MINE}, {{20, 276, 200, 316}, L"", CAR_FIND}, {{20, 328, 200, 368}, L"", CAR_MODIFY}, {{20, 380, 200, 420}, L"", CAR_DELETE}, {{20, 432, 95, 472}, L"", CAR_PREV}, {{125, 432, 200, 472}, L"", CAR_NEXT}, {{20, 492, 200, 532}, L"", CAR_BACK} }; return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y); } else { Button buttons[] = { {{20, 120, 200, 160}, L"", CAR_ADD}, {{20, 224, 200, 264}, L"", CAR_LIST_MINE}, {{20, 276, 200, 316}, L"", CAR_FIND}, {{20, 328, 200, 368}, L"", CAR_MODIFY}, {{20, 380, 200, 420}, L"", CAR_DELETE}, {{20, 432, 95, 472}, L"", CAR_PREV}, {{125, 432, 200, 472}, L"", CAR_NEXT}, {{20, 492, 200, 532}, L"", CAR_BACK} }; return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y); } }
