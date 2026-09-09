#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui_car.c —— 车辆页面
 *
 * 本文件把 GUI 输入转换成业务函数参数。操作成功后保存 cars.txt 并记录
 * 日志；删除车辆还要保存被级联修改的保单和理赔文件。
 */

static const Button CAR_ADMIN_BUTTONS[] = {{{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                                           {{20, 172, 200, 212}, L"查看全部", CAR_LIST_ALL},
                                           {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                                           {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                                           {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                                           {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                                           {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                                           {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                                           {{20, 492, 200, 532}, L"返回", CAR_BACK}};

static const Button CAR_USER_BUTTONS[] = {{{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                                          {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                                          {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                                          {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                                          {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                                          {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                                          {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                                          {{20, 492, 200, 532}, L"返回", CAR_BACK}};

static const Button *car_buttons(const AppContext *ctx, int *count)
{
    if (app_is_admin(ctx))
    {
        *count = (int)(sizeof(CAR_ADMIN_BUTTONS) / sizeof(CAR_ADMIN_BUTTONS[0]));
        return CAR_ADMIN_BUTTONS;
    }
    *count = (int)(sizeof(CAR_USER_BUTTONS) / sizeof(CAR_USER_BUTTONS[0]));
    return CAR_USER_BUTTONS;
}

/* 把一个 Car 格式化为表格中的宽字符串；count 是目标缓冲区容量。 */
static void format_car_line(const Car *car, wchar_t *line, size_t count)
{
    wchar_t plate[32], brand[64], model[64], owner[64];
    char_to_wchar(car->plate, plate, 32);
    char_to_wchar(car->brand, brand, 64);
    char_to_wchar(car->model, model, 64);
    char_to_wchar(car->owner, owner, 64);
    _snwprintf_s(line, count, _TRUNCATE, L"%-8s %-10s %-10s %-10s %-8s %04d-%02d-%02d %.2f", plate,
                 brand, model, owner, violation_to_text(car->violation), car->purchase_date.year,
                 car->purchase_date.month, car->purchase_date.day, car->purchase_price);
}

/* 页面层的薄包装，实际权限过滤由 car.c 负责。 */
/*
 * 绘制车辆页并完成分页。total_pages 使用“向上取整除法”：
 * (total + PAGE_SIZE - 1) / PAGE_SIZE，保证不足一页的数据也占一页。
 */
void draw_cars(const AppContext *ctx, const GuiState *state)
{
    int i;
    int button_count;
    const Button *buttons = car_buttons(ctx, &button_count);
    draw_title(L"车辆管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < button_count; ++i)
        draw_button(&buttons[i]);
    draw_content_panel();
    /* 表头顺序必须与 format_car_line 输出字段一致。 */
    draw_table_header(
        L"车牌    品牌        型号        车主        违章记录     购入日期     价格");
    int indices[MAX_CARS] = {0};
    int total = collect_visible_car_indices(ctx, state->car_show_mine, indices, MAX_CARS);
    PageRange page = make_page_range(state->car_page, total);
    /* lines 保存每行文字的地址，buffer 才是真正存放文字的二维数组。 */
    const wchar_t *lines[PAGE_SIZE] = {0};
    wchar_t buffer[PAGE_SIZE][256] = {0};
    int line_count = 0;
    for (i = page.start; i < page.end; ++i)
    {
        format_car_line(&ctx->cars[indices[i]], buffer[line_count], 256);
        lines[line_count] = buffer[line_count];
        ++line_count;
    }
    if (line_count == 0)
    {
        wcsncpy_s(buffer[0], 256, L"无车辆记录", _TRUNCATE);
        lines[0] = buffer[0];
        line_count = 1;
    }
    draw_rows(lines, line_count);
    draw_footer_page(page.page, total);
}

/* 按顺序收集和验证字段，再调用带权限检查的车辆新增接口。 */
static void handle_add_car(AppContext *ctx, GuiState *state)
{
    char plate[10] = {0}, brand[20] = {0}, model[20] = {0}, owner[20] = {0};
    ViolationLevel violation = VIOLATION_NONE;
    Date date = {0};
    date.year = date_today().year;
    date.month = 1;
    date.day = 1;
    double price = 0.0;
    if (!prompt_char_field(L"添加车辆", L"请输入车牌：", plate, sizeof(plate), ""))
        return;
    if (!validate_plate(plate))
    {
        set_message(state, L"车牌格式不合法：只允许字母数字和短横，长度限制请保证不超过9字符");
        return;
    }
    if (!prompt_char_field(L"添加车辆", L"请输入品牌：", brand, sizeof(brand), ""))
        return;
    if (!validate_string_len(brand, (int)sizeof(((Car *)0)->brand)))
    {
        set_message(state, L"品牌长度不合法");
        return;
    }
    if (!prompt_char_field(L"添加车辆", L"请输入型号：", model, sizeof(model), ""))
        return;
    if (!validate_string_len(model, (int)sizeof(((Car *)0)->model)))
    {
        set_message(state, L"型号长度不合法");
        return;
    }
    if (!prompt_violation_field(L"添加车辆", &violation, VIOLATION_NONE))
        return;
    if (!prompt_date_field(L"添加车辆", &date, NULL))
        return;
    if (!date_is_valid(&date))
    {
        set_message(state, L"购买日期不合法");
        return;
    }
    if (!prompt_double_field(L"添加车辆", L"请输入购入价格（元）：", &price, 0.0))
        return;
    if (!validate_price_value(price))
    {
        set_message(state, L"价格不合法（必须在 0 - 1e9 范围内）");
        return;
    }
    if (app_is_admin(ctx))
    {
        if (!prompt_char_field(L"添加车辆", L"请输入车主用户名：", owner, sizeof(owner), ""))
            return;
        if (!validate_string_len(owner, (int)sizeof(((User *)0)->username)) ||
            !user_exists(ctx, owner))
        {
            set_message(state, L"指定的车主不存在或用户名长度不合法");
            return;
        }
    }
    else
    {
        copy_text(owner, sizeof(owner), ctx->current_user);
    }
    if (add_car_for_current_user(ctx, plate, brand, model, owner, violation, date, price))
    {
        save_cars(ctx);
        append_operation_log(ctx, "ADD_CAR", plate, owner);
        set_message(state, L"添加车辆成功");
        state->car_show_mine = 0;
    }
    else
        set_message(state, L"添加车辆失败（可能车牌已存在或已达上限）");
}

/* 使用权限感知的 find_visible_car，普通用户无法查询他人车辆。 */
static void handle_find_car(AppContext *ctx, GuiState *state)
{
    char plate[10] = {0};
    if (!prompt_char_field(L"查找车辆", L"请输入车牌：", plate, sizeof(plate), ""))
        return;
    const Car *car = find_visible_car(ctx, plate);
    if (!car)
    {
        set_message(state, L"未找到该车辆");
        return;
    }
    wchar_t plate_t[32], brand_t[64], model_t[64], owner_t[64], message[512];
    char_to_wchar(car->plate, plate_t, 32);
    char_to_wchar(car->brand, brand_t, 64);
    char_to_wchar(car->model, model_t, 64);
    char_to_wchar(car->owner, owner_t, 64);
    _snwprintf_s(message, 512, _TRUNCATE,
                 L"车牌:%s 品牌:%s 型号:%s 车主:%s 违章:%s 购入:%04d-%02d-%02d 价格:%.2f元",
                 plate_t, brand_t, model_t, owner_t, violation_to_text(car->violation),
                 car->purchase_date.year, car->purchase_date.month, car->purchase_date.day,
                 car->purchase_price);
    set_message(state, message);
}

/* 先用原值作为输入框默认值，确认后统一提交修改并保存。 */
static void handle_modify_car(AppContext *ctx, GuiState *state)
{
    char plate[10] = {0};
    if (!prompt_char_field(L"编辑车辆", L"请输入要编辑的车牌：", plate, sizeof(plate), ""))
        return;
    if (!validate_plate(plate))
    {
        set_message(state, L"车牌格式不合法");
        return;
    }
    const Car *car = find_visible_car(ctx, plate);
    if (!car)
    {
        set_message(state, L"未找到该车辆");
        return;
    }
    char brand[20] = {0}, model[20] = {0};
    ViolationLevel violation = car->violation;
    Date date = car->purchase_date;
    double price = car->purchase_price;
    if (!prompt_char_field(L"编辑车辆", L"请输入新品牌（留空则保持不变）：", brand, sizeof(brand),
                           car->brand))
        return;
    if (!validate_string_len(brand, (int)sizeof(((Car *)0)->brand)))
    {
        set_message(state, L"品牌长度不合法");
        return;
    }
    if (!prompt_char_field(L"编辑车辆", L"请输入新型号（留空则保持不变）：", model, sizeof(model),
                           car->model))
        return;
    if (!validate_string_len(model, (int)sizeof(((Car *)0)->model)))
    {
        set_message(state, L"型号长度不合法");
        return;
    }
    if (!prompt_violation_field(L"编辑车辆", &violation, car->violation))
        return;
    if (!prompt_date_field(L"编辑车辆", &date, &car->purchase_date))
        return;
    if (!date_is_valid(&date))
    {
        set_message(state, L"购买日期不合法");
        return;
    }
    if (!prompt_double_field(L"编辑车辆", L"请输入新购入价格：", &price, car->purchase_price))
        return;
    if (!validate_price_value(price))
    {
        set_message(state, L"价格不合法（必须在 0 - 1e9 范围内）");
        return;
    }
    if (modify_car_for_current_user(ctx, plate, brand, model, violation, date, price))
    {
        save_cars(ctx);
        append_operation_log(ctx, "MODIFY_CAR", plate, "modified");
        set_message(state, L"编辑车辆成功");
    }
    else
        set_message(state, L"编辑车辆失败");
}

/* 先删除关联保单/理赔，再删除车辆，最后全量保存三个受影响的数据文件。 */
static void handle_delete_car(AppContext *ctx, GuiState *state)
{
    char plate[10] = {0};
    if (!prompt_char_field(L"删除车辆", L"请输入要删除的车牌：", plate, sizeof(plate), ""))
        return;
    if (!validate_plate(plate))
    {
        set_message(state, L"车牌格式不合法");
        return;
    }
    if (remove_car_for_current_user(ctx, plate))
    {
        save_cars(ctx);
        save_policies(ctx);
        save_claims(ctx);
        append_operation_log(ctx, "DELETE_CAR", plate, "deleted with cascade");
        set_message(state, L"删除车辆成功");
    }
    else
        set_message(state, L"删除车辆失败");
}

/* 根据 CarAction 执行业务操作、切换筛选、翻页或返回首页。 */
void handle_car_action(AppContext *ctx, GuiState *state, int action)
{
    int indices[MAX_CARS] = {0};
    int total = collect_visible_car_indices(ctx, state->car_show_mine, indices, MAX_CARS);
    PageRange page = make_page_range(state->car_page, total);
    switch (action)
    {
    case CAR_ADD:
        handle_add_car(ctx, state);
        break;
    case CAR_LIST_ALL:
        state->car_show_mine = 0;
        state->car_page = 0;
        set_message(state, L"显示全部车辆");
        break;
    case CAR_LIST_MINE:
        state->car_show_mine = 1;
        state->car_page = 0;
        set_message(state, L"显示我的车辆");
        break;
    case CAR_FIND:
        handle_find_car(ctx, state);
        break;
    case CAR_MODIFY:
        handle_modify_car(ctx, state);
        break;
    case CAR_DELETE:
        handle_delete_car(ctx, state);
        break;
    case CAR_PREV:
        if (state->car_page > 0)
            state->car_page--;
        break;
    case CAR_NEXT:
        if (state->car_page + 1 < page.total_pages)
            state->car_page++;
        break;
    case CAR_BACK:
        state->screen = SCREEN_HOME;
        set_message(state, L"返回主界面");
        break;
    default:
        break;
    }
}

/* 根据身份构造与画面一致的按钮矩形，并返回鼠标命中的操作编号。 */
int hit_test_cars(const AppContext *ctx, int x, int y)
{
    int count;
    const Button *buttons = car_buttons(ctx, &count);
    return hit_test_buttons(buttons, count, x, y);
}
