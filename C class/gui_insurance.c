#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"



/*
 * gui_insurance.c —— 保单与理赔页面的 GUI 控制层
 *
 * state->insurance_view 决定当前显示“保单列表”还是“理赔列表”。draw_* 只
 * 读取数据并绘制；handle_* 收集输入并调用 insurance.c；hit_test_* 把鼠标
 * 坐标转换成操作编号。权限最终还会在业务层检查，隐藏按钮不能代替鉴权。
 */

/* 一种产品的三个相关属性放进同一个结构体，避免平行数组下标错位。 */
typedef struct InsuranceProduct
{
    const wchar_t *name;
    double coverage_ratio;
    double premium_multiplier;
} InsuranceProduct;

static const InsuranceProduct INSURANCE_PRODUCTS[] = {
    {L"交强险", 0.30, 0.8},   /* 强制基础险，保额较低、保费最便宜 */
    {L"车损险", 0.80, 1.2},   /* 车辆损失险，保额接近车价 */
    {L"三者险", 1.00, 1.5},   /* 第三者责任险，保额最高 */
    {L"座位险", 0.20, 0.4},   /* 车上人员责任险，保额较低 */
    {L"盗抢险", 0.60, 0.7},   /* 全车盗抢险，中档保额 */
    {L"玻璃险", 0.05, 0.15},  /* 玻璃单独破碎险，保额最小、保费最低 */
};

#define PRODUCT_COUNT ((int)(sizeof(INSURANCE_PRODUCTS) / sizeof(INSURANCE_PRODUCTS[0])))

/*
 * 普通用户和管理员拥有不同的理赔操作权限，因此分别定义两组按钮：
 * 普通用户只能撤销自己的待审核理赔；管理员只能审核和结案，不能代用户撤销。
 * 绘制与点击检测必须取得同一组数组，否则隐藏按钮所在区域仍可能被点击。
 */
static const Button USER_INSURANCE_BUTTONS[] = {
    {{20, 120, 200, 160}, L"添加保单", INS_ADD_POLICY},
    {{20, 172, 200, 212}, L"删除保单", INS_DELETE_POLICY},
    {{20, 224, 200, 264}, L"查询保单", INS_FIND_POLICY},
    {{20, 276, 200, 316}, L"保单列表", INS_LIST_POLICIES},
    {{20, 328, 200, 368}, L"新增理赔", INS_ADD_CLAIM},
    {{20, 380, 200, 420}, L"理赔列表", INS_LIST_CLAIMS},
    {{20, 432, 200, 472}, L"撤销理赔", INS_CANCEL_CLAIM},
    {{20, 484, 95, 524}, L"上一页", INS_PREV},
    {{125, 484, 200, 524}, L"下一页", INS_NEXT},
    {{20, 536, 200, 576}, L"返回", INS_BACK}};

static const Button ADMIN_INSURANCE_BUTTONS[] = {
    {{20, 120, 200, 160}, L"添加保单", INS_ADD_POLICY},
    {{20, 172, 200, 212}, L"删除保单", INS_DELETE_POLICY},
    {{20, 224, 200, 264}, L"查询保单", INS_FIND_POLICY},
    {{20, 276, 200, 316}, L"保单列表", INS_LIST_POLICIES},
    {{20, 328, 200, 368}, L"新增理赔", INS_ADD_CLAIM},
    {{20, 380, 200, 420}, L"理赔列表", INS_LIST_CLAIMS},
    {{20, 432, 200, 472}, L"审核理赔", INS_REVIEW_CLAIM},
    {{20, 484, 200, 524}, L"结案理赔", INS_SETTLE_CLAIM},
    {{20, 536, 95, 576}, L"上一页", INS_PREV},
    {{125, 536, 200, 576}, L"下一页", INS_NEXT},
    {{20, 588, 200, 628}, L"返回", INS_BACK}};

/* 根据当前登录角色返回需要显示的按钮数组，并通过 count 返回数组长度。 */
static const Button *insurance_buttons_for_context(const AppContext *ctx, int *count)
{
    /* 返回 static 数组首地址，并通过 count 输出元素数量；调用者不能 free。
       绘制和命中检测都调用这里，隐藏的管理员/用户专属按钮不会被误点击。 */
    if (app_is_admin(ctx))
    {
        *count = (int)(sizeof(ADMIN_INSURANCE_BUTTONS) / sizeof(ADMIN_INSURANCE_BUTTONS[0]));
        return ADMIN_INSURANCE_BUTTONS;
    }
    *count = (int)(sizeof(USER_INSURANCE_BUTTONS) / sizeof(USER_INSURANCE_BUTTONS[0]));
    return USER_INSURANCE_BUTTONS;
}

/* 把一个保单转换成表格宽字符串；?: 根据布尔状态选择显示文字。 */
static void format_policy_line(const Policy *policy, wchar_t *line, size_t count)
{
    /* Policy 中的 char 字段先分别转换为宽字符。line/count 是调用者提供的
       最终行缓冲区及容量，本函数把全部字段格式化到这一行。 */
    wchar_t policy_id[40], plate[32], owner[64], desc[96];
    char_to_wchar(policy->policy_id, policy_id, 40);
    char_to_wchar(policy->plate, plate, 32);
    char_to_wchar(policy->owner, owner, 64);
    char_to_wchar(policy->coverage_desc, desc, 96);
    /* %-10s 表示左对齐并占至少 10 个字符位置，%.2f 保留两位小数；
       _TRUNCATE 确保内容过长时不会越过 line 的容量。 */
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-8s %-10s %.2f %.2f %-8s %s", policy_id, plate,
                 owner, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"无效",
                 desc);
}

/* 将数值状态转换为给用户阅读的中文文本。 */
static const wchar_t *claim_status_text(int status)
{
    switch (status)
    {
    case CLAIM_APPROVED:
        return L"已通过";
    case CLAIM_SETTLED:
        return L"已结案";
    case CLAIM_REJECTED:
        return L"已驳回";
    case CLAIM_CANCELLED:
        return L"已撤销";
    default:
        return L"待审核";
    }
}

/* 把一个理赔记录转换成表格宽字符串。 */
static void format_claim_line(const Claim *claim, wchar_t *line, size_t count)
{
    wchar_t claim_id[40], policy_id[40], plate[32], claimant[64], desc[96];
    char_to_wchar(claim->claim_id, claim_id, 40);
    char_to_wchar(claim->policy_id, policy_id, 40);
    char_to_wchar(claim->plate, plate, 32);
    char_to_wchar(claim->claimant, claimant, 64);
    char_to_wchar(claim->description, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-10s %-8s %-10s %.2f %.2f %-8s %s", claim_id,
                 policy_id, plate, claimant, claim->request_amount, claim->approved_amount,
                 claim_status_text(claim->status), desc);
}



/*
 * 绘制当前保险视图。先取得用户可见数据的原数组下标，再按页填充 buffer，
 * lines 只保存各行缓冲区地址，最后统一交给 draw_rows。
 */
void draw_insurance(const AppContext *ctx, const GuiState *state)
{
    int i;
    int button_count;
    const Button *buttons = insurance_buttons_for_context(ctx, &button_count);

    draw_title(L"保单与理赔管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < button_count; ++i)
        draw_button(&buttons[i]);
    draw_content_panel();

    /* 两个列表是互斥分支，共用页面外框和按钮，但拥有各自的页码、容量和
       格式化函数。 */
    if (state->insurance_view == VIEW_POLICY_LIST)
    {
        draw_table_header(L"保单号    车牌    投保人      保额     保费   状态   描述");
        /* buffer 真正保存每行字符；lines 只保存各行首地址，以符合 draw_rows
           接收“字符串指针数组”的接口。二者生命周期都持续到本次绘制结束。 */
        const wchar_t *lines[PAGE_SIZE] = {0};
        wchar_t buffer[PAGE_SIZE][256] = {0};
        int indices[MAX_POLICIES] = {0};
        int total = collect_visible_policy_indices(ctx, indices, MAX_POLICIES);
        PageRange page = make_page_range(state->policy_page, total);
        int row_count = 0;
        if (total == 0)
        {
            wcsncpy_s(buffer[0], 256, L"无保单记录", _TRUNCATE);
            lines[0] = buffer[0];
            draw_rows(lines, 1);
            draw_footer_page(0, 0);
            return;
        }
        /* i 遍历筛选结果位置，indices[i] 才是 ctx->policies 中的真实下标。 */
        for (i = page.start; i < page.end; ++i)
        {
            const Policy *policy = &ctx->policies[indices[i]];
            format_policy_line(policy, buffer[row_count], 256);
            lines[row_count] = buffer[row_count];
            ++row_count;
        }
        draw_rows(lines, row_count);//传入每行的首地址
        draw_footer_page(page.page, total);
    }
    else
    {
        draw_table_header(L"理赔号    保单号    车牌    申请人    申请(元)  批准(元)  状态   描述");
        /* 理赔分支使用相同的“下标数组 + 行缓冲区 + 指针数组”绘制机制。 */
        const wchar_t *lines[PAGE_SIZE] = {0};
        wchar_t buffer[PAGE_SIZE][256] = {0};
        int indices[MAX_CLAIMS] = {0};
        int total = collect_visible_claim_indices(ctx, indices, MAX_CLAIMS);
        PageRange page = make_page_range(state->claim_page, total);
        int row_count = 0;
        if (total == 0)
        {
            wcsncpy_s(buffer[0], 256, L"无理赔记录", _TRUNCATE);
            lines[0] = buffer[0];
            draw_rows(lines, 1);
            draw_footer_page(0, 0);
            return;
        }
        for (i = page.start; i < page.end; ++i)
        {
            const Claim *claim = &ctx->claims[indices[i]];
            format_claim_line(claim, buffer[row_count], 256);
            lines[row_count] = buffer[row_count];
            ++row_count;
        }
        draw_rows(lines, row_count);
        draw_footer_page(page.page, total);
    }
}

/*
 * 模态产品选择器。内部 while(1) 等待鼠标点击；产品按钮 id 从 1 开始编号，取消
 *
 * id 为 0，因此返回值既能表示选择结果，也能表示取消。
 */
static int prompt_product_choice(const wchar_t *title, int default_choice)
{
    /* 对话框尺寸固定，再根据内容区域边界计算居中坐标。 */
    int dialog_w = 560;
    int dialog_h = 300;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    /* 先画背景，再画标题和说明，后绘制的内容会覆盖在先绘制内容上方。 */
    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
    /* RECT 限定标题绘制区域，DT_VCENTER 让文字在矩形中垂直居中。 */
    settextstyle(22, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    RECT title_rect = {dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48};
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    /* 说明文字使用独立矩形，避免和标题或按钮重叠。 */
    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = {dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 84};
    draw_text_rect(L"请选择保险产品：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    /* 产品按钮按 3 列网格排列；数组额外留一个元素作为取消按钮。 */
    const int COLS = 3;
    int total_btn = PRODUCT_COUNT + 1; /* 最后一个元素是取消。 */
    int btn_w = 150;
    int btn_h = 44;
    int spacing = 24;
    int grid_w = COLS * btn_w + (COLS - 1) * spacing;
    int start_x = dlg_left + (dialog_w - grid_w) / 2;
    int btn_y0 = dlg_top + 100;
    int row_pitch = btn_h + 12;

    Button btns[PRODUCT_COUNT + 1];
    /* i / COLS 和 i % COLS 分别计算行列。产品 id 从 1 开始，便于用 0 表示取消。 */
    for (int i = 0; i < PRODUCT_COUNT; ++i)
    {
        int row = i / COLS;
        int col = i % COLS;
        btns[i].rect.left = start_x + col * (btn_w + spacing);
        btns[i].rect.top = btn_y0 + row * row_pitch;
        btns[i].rect.right = btns[i].rect.left + btn_w;
        btns[i].rect.bottom = btns[i].rect.top + btn_h;
        btns[i].label = INSURANCE_PRODUCTS[i].name;
        btns[i].id = i + 1;
        draw_button(&btns[i]);
    }
    /* 取消按钮放在产品网格下方中央，id 固定为 0。 */
    int ci = PRODUCT_COUNT;
    int cancel_row = (PRODUCT_COUNT + COLS - 1) / COLS;
    btns[ci].rect.left = dlg_left + (dialog_w - btn_w) / 2;
    btns[ci].rect.top = btn_y0 + cancel_row * row_pitch;
    btns[ci].rect.right = btns[ci].rect.left + btn_w;
    btns[ci].rect.bottom = btns[ci].rect.top + btn_h;
    btns[ci].label = L"取消";
    btns[ci].id = 0;
    draw_button(&btns[ci]);

    /* 默认项只改变显示样式，并不会自动返回；用户仍需实际点击一个按钮。 */
    if (default_choice > 0 && default_choice <= PRODUCT_COUNT)
    {
        setfillcolor(RGB(230, 245, 255));
        setlinecolor(RGB(100, 140, 180));
        solidrectangle(btns[default_choice - 1].rect.left, btns[default_choice - 1].rect.top,
                       btns[default_choice - 1].rect.right, btns[default_choice - 1].rect.bottom);
        settextcolor(RGB(20, 60, 100));
        settextstyle(18, 0, L"Segoe UI");
        draw_text_rect(btns[default_choice - 1].label, btns[default_choice - 1].rect,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    FlushBatchDraw();

    /* 模态等待：只处理本对话框的鼠标点击，选中或取消后才返回外层页面。 */
    ExMessage msg;
    while (1)
    {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN)
        {
            int mx = msg.x;
            int my = msg.y;
            for (int i = 0; i < total_btn; ++i)
            {
                if (point_in_rect(mx, my, &btns[i].rect))
                {
                    /* 返回前重画一次按下状态，并立即刷新，让用户看到点击反馈。 */
                    setlinecolor(RGB(80, 90, 110));
                    setfillcolor(RGB(220, 230, 240));
                    solidrectangle(btns[i].rect.left, btns[i].rect.top, btns[i].rect.right,
                                   btns[i].rect.bottom);
                    settextcolor(RGB(35, 40, 50));
                    settextstyle(18, 0, L"Segoe UI");
                    draw_text_rect(btns[i].label, btns[i].rect,
                                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    FlushBatchDraw();
                    /* 返回 id 后局部按钮数组失效，但调用者只需要这个整数。 */
                    return btns[i].id;
                }
            }
            /* 点击对话框外等价于取消，避免没有明确确认却继续添加保单。 */
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom)
            {
                return 0;
            }
        }
        /* 没有鼠标消息时让出 CPU，防止 while(1) 忙等。 */
        Sleep(10);
    }
}

/*
 * 新增保单完整流程：输入编号、车牌和车主，选择保险产品与有效期，再把这些
 * 数据组合成 PolicyTerms。业务层完成查重、权限校验、保额和保费计算；
 * GUI 只在操作成功后保存数据并记录日志。
 */
/* 在日期上增加天数，自动处理跨月、跨年。 */
static Date date_plus_days(Date base, int days)
{
    while (days > 0)
    {
        int dim = date_days_in_month(base.year, base.month);
        if (base.day < dim)
            base.day++;
        else if (base.month < 12)
        {
            base.day = 1;
            base.month++;
        }
        else
        {
            base.day = 1;
            base.month = 1;
            base.year++;
        }
        --days;
    }
    return base;
}

/* 在日期上增加月份，自动把月末天数收敛到当月最大值。 */
static Date date_plus_months(Date base, int months)
{
    base.month += months;
    while (base.month > 12)
    {
        base.month -= 12;
        base.year++;
    }
    int dim = date_days_in_month(base.year, base.month);
    if (base.day > dim)
        base.day = dim;
    return base;
}

/*
 * 处理“添加保单”的完整 GUI 流程。
 *
 * INS_ADD_POLICY -> handle_insurance_action -> handle_add_policy
 *                -> add_policy_for_current_user
 *
 * 前半段只把输入保存在局部变量中，不修改 AppContext。全部字段通过校验并且
 * 用户确认后，才调用业务函数一次性写入 ctx->policies；随后保存并写日志。
 */
static void handle_add_policy(AppContext *ctx, GuiState *state)
{
    /* char 数组用于业务结构体；wchar_t 数组只用于 EasyX 的确认文字。={0}
       清空数组，使尚未写入内容的数组也一定是合法的空字符串。 */
    char policy_id[24] = {0};
    char plate[10] = {0};
    char owner[20] = {0};
    wchar_t policy_text[64] = {0}, plate_text[32] = {0}, owner_text[64] = {0};
    wchar_t product_text[128] = {0}, confirm_message[512] = {0};
    /* 这两个变量是确认框中的预览值。业务层仍会重新计算，避免调用者通过
       篡改 GUI 预览值来写入不符合规则的金额。 */
    double coverage_limit;
    double premium;
    if (!prompt_char_field(L"添加保单", L"请输入保单号：", policy_id, sizeof(policy_id), ""))
        return;
    if (!validate_string_len(policy_id, (int)sizeof(((Policy *)0)->policy_id)))
    {
        set_message(state, L"保单号长度不合法");
        return;
    }
    if (!prompt_char_field(L"添加保单", L"请输入投保车牌：", plate, sizeof(plate), ""))
        return;
    if (!validate_plate_key(plate))
    {
        set_message(state, L"车牌格式不合法");
        return;
    }
    /* 保单依赖车辆存在。find_car 返回车辆数组内部的地址，NULL 表示未找到；
       它不是新分配的内存，所以本函数不能 free(car)。 */
    Car *car = find_car(ctx, plate);
    if (!car)
    {
        set_message(state, L"未找到该车牌");
        return;
    }
    /* 管理员可以指定投保人；普通用户不能输入 owner，而是固定使用当前登录名。
       业务层还会再次核对 owner 与车辆所有者，GUI 检查不是唯一安全边界。 */
    if (app_is_admin(ctx))
    {
        if (!prompt_char_field(L"添加保单", L"请输入投保人用户名：", owner, sizeof(owner), ""))
            return;
        if (!validate_string_len(owner, (int)sizeof(((User *)0)->username)) ||
            !user_exists(ctx, owner))
        {
            set_message(state, L"指定的用户不存在或用户名长度不合法");
            return;
        }
    }
    else
    {
        copy_text(owner, sizeof(owner), ctx->current_user);
    }
    if (strcmp(owner, car->owner) != 0)
    {
        set_message(state, L"投保人必须为车辆所有者");
        return;
    }

    /* 返回值从 1 开始，减 1 后才是 INSURANCE_PRODUCTS 的数组下标。 */
    int choice = prompt_product_choice(L"选择保险产品", 1);
    if (choice <= 0 || choice > PRODUCT_COUNT)
    {
        set_message(state, L"已取消产品选择");
        return;
    }

    /* 保险有效期固定为一年，只选择生效日期；生效日限定在
       “今天 + 10 天”到“今天 + 1 个月”之间，默认取今天 + 10 天。 */
    Date today = date_today();
    Date min_date = date_plus_days(today, 10);
    Date max_date = date_plus_months(today, 1);
    Date start = min_date;
    if (!prompt_date_field_bounded(L"保单生效日", &start, &start, &min_date, &max_date))
        return;
    Date end = date_plus_months(start, 12);
    char product_desc[64] = {0};
    wchar_to_char(INSURANCE_PRODUCTS[choice - 1].name, product_desc, sizeof(product_desc));
    /* PolicyTerms 把描述、日期和倍率作为一个整体传递，避免参数过多时顺序
       传错。description 的文字会由业务层复制进 Policy，而不是保存该指针。 */
    PolicyTerms terms;
    terms.description = product_desc;
    terms.start_date = start;
    terms.end_date = end;
    terms.coverage_ratio = INSURANCE_PRODUCTS[choice - 1].coverage_ratio;
    terms.premium_multiplier = INSURANCE_PRODUCTS[choice - 1].premium_multiplier;

    /* 按业务层相同公式预览保额和保费，让用户在真正添加前核对结果。 */
    coverage_limit = car->purchase_price * terms.coverage_ratio;
    if (coverage_limit <= 0.0)
        coverage_limit = 5000.0;
    premium = calculate_premium_for_car(car) * terms.premium_multiplier;
    char_to_wchar(policy_id, policy_text, 64);
    char_to_wchar(plate, plate_text, 32);
    char_to_wchar(owner, owner_text, 64);
    char_to_wchar(product_desc, product_text, 128);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"请核对即将添加的保单：\n\n保单号：%s    车牌：%s\n投保人：%s    产品：%s\n"
                 L"保额：%.2f 元    保费：%.2f 元\n有效期：%04d-%02d-%02d 至 %04d-%02d-%02d",
                 policy_text, plate_text, owner_text, product_text, coverage_limit, premium,
                 start.year, start.month, start.day, end.year, end.month, end.day);
    if (!show_confirm_dialog(L"确认添加保单", confirm_message, L"确认添加", 0))
    {
        set_message(state, L"已取消添加保单");
        return;
    }
    /* 后续确认框、保单和日志都使用车辆记录中的规范车牌。 */
    copy_text(plate, sizeof(plate), car->plate);

    /* &terms 是结构体地址。业务函数返回 1 时，保单数组和 policy_count 已经
       更新；这时才保存文件并写成功日志。返回 0 时不执行这些副作用。 */
    if (add_policy_for_current_user(ctx, policy_id, plate, owner, &terms))
    {
        save_policies(ctx);
        append_operation_log(ctx, "ADD_POLICY", policy_id, product_desc);
        set_message(state, L"添加保单成功");
        state->insurance_view = VIEW_POLICY_LIST;
        state->policy_page = 0;
    }
    else
        set_message(state, L"添加保单失败（可能已存在或达到上限）");
}

/* 删除保单前检查存在性和所有权；管理员可以操作任意保单。 */
static void handle_delete_policy(AppContext *ctx, GuiState *state)
{
    char policy_id[24] = {0};
    const Policy *policy;
    wchar_t policy_text[64] = {0}, plate_text[32] = {0}, owner_text[64] = {0};
    wchar_t confirm_message[512] = {0};

    if (!prompt_char_field(L"删除保单", L"请输入保单号：", policy_id, sizeof(policy_id), ""))
        return;
    /* 权限感知查找：存在但不属于普通用户的保单也返回 NULL。返回地址只用于
       删除前展示，真正删除后数组会移动，不能再使用该指针。 */
    policy = find_visible_policy(ctx, policy_id);
    if (!policy)
    {
        set_message(state, L"未找到该保单或无权删除");
        return;
    }

    char_to_wchar(policy->policy_id, policy_text, 64);
    char_to_wchar(policy->plate, plate_text, 32);
    char_to_wchar(policy->owner, owner_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"确定删除这份保单吗？\n\n保单号：%s\n关联车辆：%s    投保人：%s\n\n"
                 L"警告：该保单关联的理赔记录也会被删除，此操作无法撤销。",
                 policy_text, plate_text, owner_text);
    if (!show_confirm_dialog(L"删除保单", confirm_message, L"确认删除", 1))
    {
        set_message(state, L"已取消删除保单");
        return;
    }

    /* 删除保单会级联删除关联理赔，所以成功后必须同时保存两个文件。 */
    if (remove_policy_for_current_user(ctx, policy_id))
    {
        save_policies(ctx);
        save_claims(ctx);
        append_operation_log(ctx, "DELETE_POLICY", policy_id, "deleted by user");
        set_message(state, L"删除保单成功");
    }
    else
        set_message(state, L"删除保单失败");
}

/* 查询结果同时显示保单基本信息和生效日期，便于用户核对起保时间。 */
static void handle_find_policy(AppContext *ctx, GuiState *state)
{
    char policy_id[24] = {0};
    if (!prompt_char_field(L"查询保单", L"请输入保单号：", policy_id, sizeof(policy_id), ""))
        return;
    /* const Policy* 表明这里只读取查询结果来组织消息，不允许借此修改保单。 */
    const Policy *policy = find_visible_policy(ctx, policy_id);
    if (!policy)
    {
        set_message(state, L"未找到保单");
        return;
    }
    wchar_t id_t[40], plate_t[32], owner_t[64], desc_t[96], message[512];
    char_to_wchar(policy->policy_id, id_t, 40);
    char_to_wchar(policy->owner, owner_t, 64);
    char_to_wchar(policy->plate, plate_t, 32);
    char_to_wchar(policy->coverage_desc, desc_t, 96);
    _snwprintf_s(message, 512, _TRUNCATE,
                 L"保单:%s  车牌:%s  投保人:%s  保额:%.2f  保费:%.2f  状态:%s\n"
                 L"生效日期:%04d-%02d-%02d  描述:%s",
                 id_t, plate_t, owner_t, policy->coverage_limit, policy->premium,
                 policy->active ? L"有效" : L"失效", policy->start_date.year,
                 policy->start_date.month, policy->start_date.day, desc_t);
    set_message(state, message);
}

/* 收集理赔资料并调用权限包装接口；批准金额由 insurance.c 自动计算。 */
static void handle_add_claim(AppContext *ctx, GuiState *state)
{
    /* 输入阶段使用 char 局部数组；确认摘要使用转换后的 wchar_t 数组。所有
       明文说明只在确认后才交给业务层复制。 */
    char claim_id[24] = {0}, policy_id[24] = {0}, desc[256] = {0}, claimant[20] = {0};
    double amount = 0.0;
    wchar_t claim_text[64] = {0}, policy_text[64] = {0}, claimant_text[64] = {0};
    wchar_t desc_text[256] = {0}, confirm_message[512] = {0};
    if (!prompt_char_field(L"新增理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), ""))
        return;
    if (!validate_string_len(claim_id, (int)sizeof(((Claim *)0)->claim_id)))
    {
        set_message(state, L"理赔单号长度不合法");
        return;
    }
    if (!prompt_char_field(L"新增理赔", L"请输入关联保单号：", policy_id, sizeof(policy_id), ""))
        return;
    if (!validate_string_len(policy_id, (int)sizeof(((Policy *)0)->policy_id)))
    {
        set_message(state, L"保单号格式不合法");
        return;
    }
    if (!prompt_double_field(L"新增理赔", L"请输入申请金额（元）：", &amount, 1000.0))
        return;
    if (amount <= 0.0 || amount > 1e9)
    {
        set_message(state, L"申请金额不合法（必须大于0且小于1e9）");
        return;
    }
    if (!prompt_char_field(L"新增理赔", L"请输入理赔说明：", desc, sizeof(desc), ""))
        return;
    if (!validate_string_len(desc, (int)sizeof(((Claim *)0)->description)))
    {
        set_message(state, L"理赔说明长度不合法");
        return;
    }
    /* 管理员可以代已存在用户提交；普通用户申请人固定为 current_user。
       关联保单所有权和有效状态仍由 add_claim_for_current_user 检查。 */
    if (app_is_admin(ctx))
    {
        if (!prompt_char_field(L"新增理赔", L"请输入申请人用户名：", claimant, sizeof(claimant),
                               ""))
            return;
        if (!validate_string_len(claimant, (int)sizeof(((User *)0)->username)) ||
            !user_exists(ctx, claimant))
        {
            set_message(state, L"指定的用户不存在或用户名长度不合法");
            return;
        }
    }
    else
    {
        copy_text(claimant, sizeof(claimant), ctx->current_user);
    }

    char_to_wchar(claim_id, claim_text, 64);
    char_to_wchar(policy_id, policy_text, 64);
    char_to_wchar(claimant, claimant_text, 64);
    char_to_wchar(desc, desc_text, 256);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"请核对即将提交的理赔：\n\n理赔号：%s    保单号：%s\n申请人：%s\n"
                 L"申请金额：%.2f 元\n事故说明：%.80s",
                 claim_text, policy_text, claimant_text, amount, desc_text);
    if (!show_confirm_dialog(L"确认提交理赔", confirm_message, L"确认提交", 0))
    {
        set_message(state, L"已取消提交理赔");
        return;
    }

    /* 返回 1 时 Claim 已写入内存并计算预计批准金额，此时才保存并切换列表。 */
    if (add_claim_for_current_user(ctx, claim_id, policy_id, claimant, desc, amount))
    {
        save_claims(ctx);
        append_operation_log(ctx, "ADD_CLAIM", claim_id, desc);
        set_message(state, L"提交理赔成功");
        state->insurance_view = VIEW_CLAIM_LIST;
        state->claim_page = 0;
    }
    else
        set_message(state, L"提交理赔失败");
}

/*
 * 撤销只允许作用于当前用户可见的待审核理赔。确认后业务层把状态改为
 * CLAIM_CANCELLED，而不是删除记录，因此以后仍能在列表中查看申请历史。
 */
static void handle_cancel_claim(AppContext *ctx, GuiState *state)
{
    char claim_id[24] = {0};
    const Claim *claim;
    wchar_t claim_text[64] = {0}, confirm_message[512] = {0};

    if (!prompt_char_field(L"撤销理赔", L"请输入要撤销的理赔单号：", claim_id,
                           sizeof(claim_id), ""))
        return;
    /* 先查可见记录并检查待审核状态，减少无效确认框；业务层仍会重复检查。 */
    claim = find_visible_claim(ctx, claim_id);
    if (!claim)
    {
        set_message(state, L"未找到该理赔单或无权操作");
        return;
    }
    if (claim->status != CLAIM_PENDING)
    {
        set_message(state, L"只有待审核理赔可以撤销");
        return;
    }

    char_to_wchar(claim->claim_id, claim_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"确定撤销理赔 %s 吗？\n\n申请金额：%.2f 元\n当前状态：待审核\n\n"
                 L"撤销后不能重新审核，但申请记录会继续保留。",
                 claim_text, claim->request_amount);
    if (!show_confirm_dialog(L"撤销待审核理赔", confirm_message, L"确认撤销", 1))
    {
        set_message(state, L"已取消撤销理赔");
        return;
    }

    /* 撤销只修改 status，不删除数组元素，因此只需保存 claims.txt。 */
    if (cancel_claim_for_current_user(ctx, claim_id))
    {
        save_claims(ctx);
        append_operation_log(ctx, "CANCEL_CLAIM", claim_id, "pending claim cancelled");
        set_message(state, L"理赔已撤销，申请记录继续保留");
        state->insurance_view = VIEW_CLAIM_LIST;
        state->claim_page = 0;
    }
    else
    {
        set_message(state, L"撤销理赔失败");
    }
}

/* 管理员审核一条待审核理赔，1 表示通过，0 表示驳回。 */
static void handle_review_claim(AppContext *ctx, GuiState *state)
{
    char claim_id[24] = {0};
    char choice[8] = {0};
    const Claim *claim;
    int approve;
    wchar_t claim_text[64] = {0}, confirm_message[512] = {0};

    if (!prompt_char_field(L"审核理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), ""))
        return;
    if (!app_is_admin(ctx))
    {
        set_message(state, L"只有管理员可以审核理赔");
        return;
    }

    claim = find_visible_claim(ctx, claim_id);
    if (!claim)
    {
        set_message(state, L"未找到该理赔单");
        return;
    }
    if (claim->status != CLAIM_PENDING)
    {
        set_message(state, L"该理赔已审核，不能重复处理");
        return;
    }
    if (!prompt_char_field(L"审核理赔", L"请输入 1=通过，0=驳回：", choice, sizeof(choice), "1"))
        return;
    if (choice[0] != '0' && choice[0] != '1')
    {
        set_message(state, L"请只输入 1 或 0");
        return;
    }

    /* 比较表达式结果是 0 或 1，正好转换为业务接口需要的 approve 标志。 */
    approve = choice[0] == '1';
    char_to_wchar(claim->claim_id, claim_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"确定将理赔 %s 标记为“%s”吗？\n\n申请金额：%.2f 元\n预计批准金额：%.2f 元\n\n"
                 L"审核结果保存后不能再次审核。",
                 claim_text, approve ? L"审核通过" : L"已驳回", claim->request_amount,
                 claim->approved_amount);
    if (!show_confirm_dialog(approve ? L"确认通过理赔" : L"确认驳回理赔", confirm_message,
                             approve ? L"确认通过" : L"确认驳回", approve ? 0 : 1))
    {
        set_message(state, L"已取消审核理赔");
        return;
    }

    /* 业务层负责最终管理员鉴权和状态转换，GUI 只在成功后保存与记日志。 */
    if (review_claim_for_current_user(ctx, claim_id, approve))
    {
        save_claims(ctx);
        append_operation_log(ctx, "REVIEW_CLAIM", claim_id, approve ? "approved" : "rejected");
        set_message(state, approve ? L"审核通过，可继续结案" : L"理赔已驳回");
    }
    else
    {
        set_message(state, L"审核理赔失败");
    }
}

/* 结案只修改理赔状态，不执行真实转账；成功后全量保存 claims.txt。 */
static void handle_settle_claim(AppContext *ctx, GuiState *state)
{
    char claim_id[24] = {0};
    const Claim *claim;
    wchar_t claim_text[64] = {0}, confirm_message[512] = {0};

    if (!prompt_char_field(L"结案理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), ""))
        return;
    if (!app_is_admin(ctx))
    {
        set_message(state, L"只有管理员可以结案理赔");
        return;
    }
    claim = find_visible_claim(ctx, claim_id);
    if (!claim)
    {
        set_message(state, L"未找到该理赔单");
        return;
    }
    if (claim->status != CLAIM_APPROVED)
    {
        set_message(state, L"该理赔尚未审核通过，不能结案");
        return;
    }
    char_to_wchar(claim->claim_id, claim_text, 64);
    _snwprintf_s(confirm_message, 512, _TRUNCATE,
                 L"确定将理赔 %s 标记为已结案吗？\n\n申请金额：%.2f 元\n批准金额：%.2f 元\n\n"
                 L"结案后不能重新审核，此操作不会发起真实转账。",
                 claim_text, claim->request_amount, claim->approved_amount);
    if (!show_confirm_dialog(L"确认理赔结案", confirm_message, L"确认结案", 1))
    {
        set_message(state, L"已取消理赔结案");
        return;
    }

    /* 结案操作只把状态从 APPROVED 改为 SETTLED，不会在本程序中进行转账。 */
    if (settle_claim_for_current_user(ctx, claim_id))
    {
        save_claims(ctx);
        append_operation_log(ctx, "SETTLE_CLAIM", claim_id, "settled by admin");
        set_message(state, L"理赔结案成功");
    }
    else
    {
        set_message(state, L"理赔结案失败");
    }
}

/*
 * 保单理赔页的统一按钮分派器。
 *
 * action 来自 hit_test_insurance 返回的 InsuranceAction。增删查等编号交给
 * 对应 handle_* 函数；列表切换和翻页只修改 GuiState；INS_BACK 把当前页面
 * 改回首页。函数不处理鼠标坐标，也不负责绘制。
 *
 * page 是 int*：显示保单时指向 state->policy_page，显示理赔时指向
 * state->claim_page。后面的同一套翻页代码通过 *page 修改真正的页码成员。
 */
void handle_insurance_action(AppContext *ctx, GuiState *state, int action)
{
    /* ?: 是条件运算符。它根据当前列表选择总记录数和页码地址，避免分别写
       两套内容完全相同的分页逻辑。 */
    int total = state->insurance_view == VIEW_POLICY_LIST ? ctx->policy_count : ctx->claim_count;
    int *page =
        state->insurance_view == VIEW_POLICY_LIST ? &state->policy_page : &state->claim_page;
    PageRange range = make_page_range(*page, total);
    /* switch 把离散的操作编号分派到对应功能。每个 case 末尾的 break 防止
       程序继续落入下一个 case。 */
    switch (action)
    {
    case INS_ADD_POLICY:
        handle_add_policy(ctx, state);
        break;
    case INS_DELETE_POLICY:
        handle_delete_policy(ctx, state);
        break;
    case INS_FIND_POLICY:
        handle_find_policy(ctx, state);
        break;
    case INS_LIST_POLICIES:
        state->insurance_view = VIEW_POLICY_LIST;
        state->policy_page = 0;
        set_message(state, L"显示保单列表");
        break;
    case INS_ADD_CLAIM:
        handle_add_claim(ctx, state);
        break;
    case INS_LIST_CLAIMS:
        state->insurance_view = VIEW_CLAIM_LIST;
        state->claim_page = 0;
        set_message(state, L"显示理赔列表");
        break;
    case INS_CANCEL_CLAIM:
        if (app_is_admin(ctx))
            set_message(state, L"管理员不能撤销用户提交的理赔");
        else
            handle_cancel_claim(ctx, state);
        break;
    case INS_REVIEW_CLAIM:
        if (app_is_admin(ctx))
            handle_review_claim(ctx, state);
        else
            set_message(state, L"只有管理员可以审核理赔");
        break;
    case INS_SETTLE_CLAIM:
        if (app_is_admin(ctx))
            handle_settle_claim(ctx, state);
        else
            set_message(state, L"只有管理员可以结案理赔");
        break;
    case INS_PREV:
        /* *page 取得指针指向的真实页码。先检查边界，保证页码不会变成负数。 */
        if (*page > 0)
            (*page)--;
        break;
    case INS_NEXT:
        /* 当前页从 0 开始，因此“当前页 + 1 < 总页数”才说明存在下一页。 */
        if (*page + 1 < range.total_pages)
            (*page)++;
        break;
    case INS_BACK:
        state->screen = SCREEN_HOME;
        set_message(state, L"返回主界面");
        break;
    default:
        break;
    }
}

/* 使用与当前角色绘制时相同的按钮数组，隐藏的操作不会参与点击检测。 */
int hit_test_insurance(const AppContext *ctx, int x, int y)
{
    int button_count;
    const Button *buttons = insurance_buttons_for_context(ctx, &button_count);
    return hit_test_buttons(buttons, button_count, x, y);
}
