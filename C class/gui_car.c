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
                                           {{20, 276, 200, 316}, L"多条件查询", CAR_FIND},
                                           {{20, 328, 200, 368}, L"车辆排序", CAR_SORT},
                                           {{20, 380, 200, 420}, L"编辑车辆", CAR_MODIFY},
                                           {{20, 432, 200, 472}, L"删除车辆", CAR_DELETE},
                                           {{20, 484, 95, 524}, L"上一页", CAR_PREV},
                                           {{125, 484, 200, 524}, L"下一页", CAR_NEXT},
                                           {{20, 544, 200, 584}, L"返回", CAR_BACK}};

static const Button CAR_USER_BUTTONS[] = {{{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                                          {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                                          {{20, 276, 200, 316}, L"多条件查询", CAR_FIND},
                                          {{20, 328, 200, 368}, L"车辆排序", CAR_SORT},
                                          {{20, 380, 200, 420}, L"编辑车辆", CAR_MODIFY},
                                          {{20, 432, 200, 472}, L"删除车辆", CAR_DELETE},
                                          {{20, 484, 95, 524}, L"上一页", CAR_PREV},
                                          {{125, 484, 200, 524}, L"下一页", CAR_NEXT},
                                          {{20, 544, 200, 584}, L"返回", CAR_BACK}};

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

/*
 * 根据当前界面状态决定列表的数据来源。查询开启时使用多条件筛选，否则使用
 * 原来的“全部/我的车辆”列表。绘制和翻页都调用本函数，保证两处数量一致。
 */
static int collect_displayed_car_indices(const AppContext *ctx, const GuiState *state,
                                         int *indices, int max_count)
{
    int count;

    if (state->car_search_active)
        count = collect_matching_car_indices(ctx, state->car_show_mine, &state->car_search, indices,
                                             max_count);
    else
        count = collect_visible_car_indices(ctx, state->car_show_mine, indices, max_count);

    /* 必须在分页前排序，才能保证整个查询结果有序，而不是只排列当前一页。 */
    sort_car_indices(ctx, indices, count, state->car_sort_field, state->car_sort_ascending);
    return count;
}

/* 清除查询状态；“查看全部”和“查看我的”使用它恢复普通列表。 */
static void clear_car_search(GuiState *state)
{
    memset(&state->car_search, 0, sizeof(state->car_search));
    state->car_search.violation = CAR_VIOLATION_ANY;
    state->car_search_active = 0;
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
    int total = collect_displayed_car_indices(ctx, state, indices, MAX_CARS);
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
    wchar_t plate_text[32] = {0}, brand_text[64] = {0}, model_text[64] = {0};
    wchar_t owner_text[64] = {0}, confirm_message[512] = {0};
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

    /*
     * 所有输入都通过验证后再显示摘要。只有确认返回 1，下面的业务函数才会
     * 修改车辆数组；取消不会保存文件，也不会写入操作日志。
     */
    char_to_wchar(plate, plate_text, 32);
    char_to_wchar(brand, brand_text, 64);
    char_to_wchar(model, model_text, 64);
    char_to_wchar(owner, owner_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"请核对即将添加的车辆：\n\n车牌：%s    品牌：%s\n型号：%s    车主：%s\n"
                 L"违章等级：%s\n购入日期：%04d-%02d-%02d    价格：%.2f 元",
                 plate_text, brand_text, model_text, owner_text, violation_to_text(violation),
                 date.year, date.month, date.day, price);
    if (!show_confirm_dialog(L"确认添加车辆", confirm_message, L"确认添加", 0))
    {
        set_message(state, L"已取消添加车辆");
        return;
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

/*
 * 依次收集车牌、品牌、型号、车主和违章等级。每个字符串都允许留空，留空表示
 * 不限制该字段；多个已填写条件必须同时满足。普通用户不询问车主，因为业务层
 * 本来就会把结果限制在当前用户名下，避免通过查询看到他人车辆。
 */
static void handle_find_car(AppContext *ctx, GuiState *state)
{
    CarSearchCondition condition;
    char violation_text[2] = {0};
    wchar_t message[128] = {0};
    int total;
    int indices[MAX_CARS] = {0};

    memset(&condition, 0, sizeof(condition));
    condition.violation = CAR_VIOLATION_ANY;
    if (state->car_search_active)
        condition = state->car_search;

    if (!prompt_optional_char_field(L"多条件查询", L"车牌关键字（留空=不限）：",
                                    condition.plate_keyword, sizeof(condition.plate_keyword),
                                    condition.plate_keyword))
        return;
    if (!prompt_optional_char_field(L"多条件查询", L"品牌关键字（留空=不限）：",
                                    condition.brand_keyword, sizeof(condition.brand_keyword),
                                    condition.brand_keyword))
        return;
    if (!prompt_optional_char_field(L"多条件查询", L"型号关键字（留空=不限）：",
                                    condition.model_keyword, sizeof(condition.model_keyword),
                                    condition.model_keyword))
        return;
    if (app_is_admin(ctx) &&
        !prompt_optional_char_field(L"多条件查询", L"车主关键字（留空=不限）：",
                                    condition.owner_keyword, sizeof(condition.owner_keyword),
                                    condition.owner_keyword))
        return;

    /* 输入框只接收一个字符：空串代表不限，0~5 与 ViolationLevel 枚举对应。 */
    if (condition.violation != CAR_VIOLATION_ANY)
    {
        violation_text[0] = (char)('0' + condition.violation);
        violation_text[1] = '\0';
    }
    if (!prompt_optional_char_field(
            L"多条件查询",
            L"违章等级（留空=不限，0=无，1=较轻微，2=轻微，3=中等，4=较严重，5=严重）：",
            violation_text, sizeof(violation_text), violation_text))
        return;
    if (violation_text[0] != '\0')
    {
        if (violation_text[1] != '\0' || violation_text[0] < '0' || violation_text[0] > '5')
        {
            set_message(state, L"违章等级输入错误：请留空或输入 0~5");
            return;
        }
        condition.violation = violation_text[0] - '0';
    }
    else
    {
        condition.violation = CAR_VIOLATION_ANY;
    }

    state->car_search = condition;
    state->car_search_active = 1;
    state->car_show_mine = app_is_admin(ctx) ? 0 : 1;
    state->car_page = 0;
    total = collect_displayed_car_indices(ctx, state, indices, MAX_CARS);
    _snwprintf_s(message, 128, _TRUNCATE, L"多条件查询完成，共找到 %d 条车辆记录", total);
    set_message(state, message);
}

/*
 * 车辆排序只保存显示规则，不修改任何车辆数据。0 恢复登记顺序；选择其他
 * 字段后再输入升序或降序。排序会同时作用于普通列表和多条件查询结果。
 */
static void handle_sort_cars(GuiState *state)
{
    char field_text[8] = {0};
    char direction_text[8] = {0};
    CarSortField field;
    const wchar_t *field_name;
    wchar_t message[128] = {0};

    if (!prompt_char_field(L"车辆排序",
                           L"请选择字段：0=恢复默认，1=违章等级，2=购入日期，3=购入价格：",
                           field_text, sizeof(field_text), "0"))
        return;
    if (field_text[1] != '\0' || field_text[0] < '0' || field_text[0] > '3')
    {
        set_message(state, L"排序字段输入错误：请输入 0~3");
        return;
    }
    if (field_text[0] == '0')
    {
        state->car_sort_field = CAR_SORT_DEFAULT;
        state->car_sort_ascending = 1;
        state->car_page = 0;
        set_message(state, L"已恢复车辆默认登记顺序");
        return;
    }

    field = (CarSortField)(field_text[0] - '0');
    if (!prompt_char_field(L"车辆排序", L"请选择方向：1=升序，2=降序：", direction_text,
                           sizeof(direction_text), "1"))
        return;
    if (direction_text[1] != '\0' || (direction_text[0] != '1' && direction_text[0] != '2'))
    {
        set_message(state, L"排序方向输入错误：请输入 1 或 2");
        return;
    }

    field_name = field == CAR_SORT_VIOLATION
                     ? L"违章等级"
                     : (field == CAR_SORT_PURCHASE_DATE ? L"购入日期" : L"购入价格");
    state->car_sort_field = field;
    state->car_sort_ascending = direction_text[0] == '1';
    state->car_page = 0;
    _snwprintf_s(message, 128, _TRUNCATE, L"当前排序：%s（%s）", field_name,
                 state->car_sort_ascending ? L"升序" : L"降序");
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
    const Car *car;
    wchar_t plate_text[32] = {0}, brand_text[64] = {0}, model_text[64] = {0};
    wchar_t owner_text[64] = {0}, confirm_message[512] = {0};

    if (!prompt_char_field(L"删除车辆", L"请输入要删除的车牌：", plate, sizeof(plate), ""))
        return;
    if (!validate_plate(plate))
    {
        set_message(state, L"车牌格式不合法");
        return;
    }
    car = find_visible_car(ctx, plate);
    if (!car)
    {
        set_message(state, L"未找到该车辆或无权删除");
        return;
    }

    char_to_wchar(car->plate, plate_text, 32);
    char_to_wchar(car->brand, brand_text, 64);
    char_to_wchar(car->model, model_text, 64);
    char_to_wchar(car->owner, owner_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"确定删除这辆车吗？\n\n车牌：%s    品牌：%s\n型号：%s    车主：%s\n\n"
                 L"警告：关联保单和理赔记录也会被删除，此操作无法撤销。",
                 plate_text, brand_text, model_text, owner_text);
    if (!show_confirm_dialog(L"删除车辆", confirm_message, L"确认删除", 1))
    {
        set_message(state, L"已取消删除车辆");
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
    int total = collect_displayed_car_indices(ctx, state, indices, MAX_CARS);
    PageRange page = make_page_range(state->car_page, total);
    switch (action)
    {
    case CAR_ADD:
        handle_add_car(ctx, state);
        break;
    case CAR_LIST_ALL:
        clear_car_search(state);
        state->car_show_mine = 0;
        state->car_page = 0;
        set_message(state, L"显示全部车辆");
        break;
    case CAR_LIST_MINE:
        clear_car_search(state);
        state->car_show_mine = 1;
        state->car_page = 0;
        set_message(state, L"显示我的车辆");
        break;
    case CAR_FIND:
        handle_find_car(ctx, state);
        break;
    case CAR_SORT:
        handle_sort_cars(state);
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
