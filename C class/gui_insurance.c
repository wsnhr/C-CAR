#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui_insurance.c —— 保单与理赔页面
 *
 * 页面同时管理两类列表，通过
 * state->insurance_view 决定当前显示哪一种。
 *
 * 本文件收集输入和更新界面；保险关联校验、权限和金额计算由 insurance.c

 * 完成；成功后再调用 storage.c 的保存函数持久化。
 */

/* 一种产品的三个相关属性放进同一个结构体，避免平行数组下标错位。 */
typedef struct InsuranceProduct
{
    const wchar_t *name;
    double coverage_ratio;
    double premium_multiplier;
} InsuranceProduct;

static const InsuranceProduct INSURANCE_PRODUCTS[] = {
    {L"交强险", 0.30, 0.8}, {L"车损险", 0.80, 1.2}, {L"三者险", 1.00, 1.5}};

#define PRODUCT_COUNT ((int)(sizeof(INSURANCE_PRODUCTS) / sizeof(INSURANCE_PRODUCTS[0])))

/* 保险页按钮只定义一次，绘制和命中检测共同使用。 */
static const Button INSURANCE_BUTTONS[] = {{{20, 120, 200, 160}, L"添加保单", INS_ADD_POLICY},
                                           {{20, 172, 200, 212}, L"删除保单", INS_DELETE_POLICY},
                                           {{20, 224, 200, 264}, L"查询保单", INS_FIND_POLICY},
                                           {{20, 276, 200, 316}, L"保单列表", INS_LIST_POLICIES},
                                           {{20, 328, 200, 368}, L"新增理赔", INS_ADD_CLAIM},
                                           {{20, 380, 200, 420}, L"理赔列表", INS_LIST_CLAIMS},
                                           {{20, 432, 200, 472}, L"撤销理赔", INS_CANCEL_CLAIM},
                                           {{20, 484, 200, 524}, L"审核理赔", INS_REVIEW_CLAIM},
                                           {{20, 536, 200, 576}, L"结案理赔", INS_SETTLE_CLAIM},
                                           {{20, 588, 95, 628}, L"上一页", INS_PREV},
                                           {{125, 588, 200, 628}, L"下一页", INS_NEXT},
                                           {{20, 640, 200, 680}, L"返回", INS_BACK}};

/* 把一个保单转换成表格宽字符串；?: 根据布尔状态选择显示文字。 */
static void format_policy_line(const Policy *policy, wchar_t *line, size_t count)
{
    wchar_t policy_id[40], plate[32], owner[64], desc[96];
    char_to_wchar(policy->policy_id, policy_id, 40);
    char_to_wchar(policy->plate, plate, 32);
    char_to_wchar(policy->owner, owner, 64);
    char_to_wchar(policy->coverage_desc, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-8s %-10s %.2f %.2f %-8s %s", policy_id, plate,
                 owner, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"失效",
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
    int button_count = (int)(sizeof(INSURANCE_BUTTONS) / sizeof(INSURANCE_BUTTONS[0]));

    draw_title(L"保单与理赔管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < button_count; ++i)
        draw_button(&INSURANCE_BUTTONS[i]);
    draw_content_panel();

    if (state->insurance_view == VIEW_POLICY_LIST)
    {
        draw_table_header(L"保单号    车牌    投保人      保额     保费   状态   描述");
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
        for (i = page.start; i < page.end; ++i)
        {
            const Policy *policy = &ctx->policies[indices[i]];
            format_policy_line(policy, buffer[row_count], 256);
            lines[row_count] = buffer[row_count];
            ++row_count;
        }
        draw_rows(lines, row_count);
        draw_footer_page(page.page, total);
    }
    else
    {
        draw_table_header(L"理赔号    保单号    车牌    申请人    申请(元)  批准(元)  状态   描述");
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
 * 模态产品选择器。内部 while(1) 等待鼠标点击；产品按钮 id 为 1~3，取消
 *
 * id 为 0，因此返回值既能表示选择结果，也能表示取消。
 */
static int prompt_product_choice(const wchar_t *title, int default_choice)
{
    // dialog dimensions inside content panel
    int dialog_w = 520;
    int dialog_h = 200;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    // draw dialog background
    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
    // title
    settextstyle(22, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    RECT title_rect = {dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48};
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // description
    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = {dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 84};
    draw_text_rect(L"请选择保险产品：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // buttons: product buttons + cancel
    int total_btn = PRODUCT_COUNT + 1; // last = cancel
    int btn_w = 140;
    int btn_h = 44;
    int spacing = 18;
    int total_width = total_btn * btn_w + (total_btn - 1) * spacing;
    int start_x = dlg_left + (dialog_w - total_width) / 2;
    int btn_y = dlg_bottom - btn_h - 20;

    Button btns[8]; // enough space (PRODUCT_COUNT small)
    for (int i = 0; i < PRODUCT_COUNT; ++i)
    {
        btns[i].rect.left = start_x + i * (btn_w + spacing);
        btns[i].rect.top = btn_y;
        btns[i].rect.right = btns[i].rect.left + btn_w;
        btns[i].rect.bottom = btn_y + btn_h;
        btns[i].label = INSURANCE_PRODUCTS[i].name;
        btns[i].id = i + 1;
        draw_button(&btns[i]);
    }
    // cancel button
    int ci = PRODUCT_COUNT;
    btns[ci].rect.left = start_x + ci * (btn_w + spacing);
    btns[ci].rect.top = btn_y;
    btns[ci].rect.right = btns[ci].rect.left + btn_w;
    btns[ci].rect.bottom = btn_y + btn_h;
    btns[ci].label = L"取消";
    btns[ci].id = 0;
    draw_button(&btns[ci]);

    // 高亮默认产品，帮助用户确认当前选择。
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

    // wait for click
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
                    // small visual feedback: redraw pressed button (optional)
                    setlinecolor(RGB(80, 90, 110));
                    setfillcolor(RGB(220, 230, 240));
                    solidrectangle(btns[i].rect.left, btns[i].rect.top, btns[i].rect.right,
                                   btns[i].rect.bottom);
                    settextcolor(RGB(35, 40, 50));
                    settextstyle(18, 0, L"Segoe UI");
                    draw_text_rect(btns[i].label, btns[i].rect,
                                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    FlushBatchDraw();
                    // consume event and return id
                    return btns[i].id;
                }
            }
            // click outside: treat as cancel
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom)
            {
                return 0;
            }
        }
        Sleep(10);
    }
}

/*
 * 新增保单完整流程：输入编号、车牌和车主，选择保险产品与有效期，再把这些
 * 数据组合成 PolicyTerms。业务层完成查重、权限校验、保额和保费计算；
 * GUI 只在操作成功后保存数据并记录日志。
 */
static void handle_add_policy(AppContext *ctx, GuiState *state)
{
    char policy_id[24] = {0};
    char plate[10] = {0};
    char owner[20] = {0};
    wchar_t policy_text[64] = {0}, plate_text[32] = {0}, owner_text[64] = {0};
    wchar_t product_text[128] = {0}, confirm_message[512] = {0};
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
    if (!validate_plate(plate))
    {
        set_message(state, L"车牌格式不合法");
        return;
    }
    Car *car = find_car(ctx, plate);
    if (!car)
    {
        set_message(state, L"未找到该车牌");
        return;
    }
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

    // Use clickable product selection
    int choice = prompt_product_choice(L"选择保险产品", 1);
    if (choice <= 0 || choice > PRODUCT_COUNT)
    {
        set_message(state, L"已取消产品选择");
        return;
    }

    Date start = date_today();
    Date end = start;
    end.year = start.year + 1;
    if (!prompt_date_field(L"保单生效日", &start, &start))
        return;
    if (!prompt_date_field(L"保单失效日", &end, &end))
        return;
    if (!date_is_valid(&start) || !date_is_valid(&end) || date_compare(&start, &end) > 0)
    {
        set_message(state, L"生效/失效日期不合法");
        return;
    }
    char product_desc[64] = {0};
    wchar_to_char(INSURANCE_PRODUCTS[choice - 1].name, product_desc, sizeof(product_desc));
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

/* 使用 find_visible_policy 查询，未授权的记录与不存在的记录都显示“未找到”。 */
static void handle_find_policy(AppContext *ctx, GuiState *state)
{
    char policy_id[24] = {0};
    if (!prompt_char_field(L"查询保单", L"请输入保单号：", policy_id, sizeof(policy_id), ""))
        return;
    const Policy *policy = find_visible_policy(ctx, policy_id);
    if (!policy)
    {
        set_message(state, L"未找到保单");
        return;
    }
    wchar_t id_t[40], plate_t[32], owner_t[64], desc_t[96], message[512];
    char_to_wchar(policy->policy_id, id_t, 40);
    char_to_wchar(policy->plate, plate_t, 32);
    char_to_wchar(policy->owner, owner_t, 64);
    char_to_wchar(policy->coverage_desc, desc_t, 96);
    _snwprintf_s(message, 512, _TRUNCATE,
                 L"保单:%s 车牌:%s 投保人:%s 保额:%.2f 保费:%.2f 状态:%s 描述:%s", id_t, plate_t,
                 owner_t, policy->coverage_limit, policy->premium,
                 policy->active ? L"有效" : L"失效", desc_t);
    set_message(state, message);
}

/* 收集理赔资料并调用权限包装接口；批准金额由 insurance.c 自动计算。 */
static void handle_add_claim(AppContext *ctx, GuiState *state)
{
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
 * 分派 InsuranceAction。page 是 int*：根据当前视图指向 policy_page 或
 *
 * claim_page，之后同一套翻页代码可修改正确的页码成员。
 */
void handle_insurance_action(AppContext *ctx, GuiState *state, int action)
{
    int total = state->insurance_view == VIEW_POLICY_LIST ? ctx->policy_count : ctx->claim_count;
    int *page =
        state->insurance_view == VIEW_POLICY_LIST ? &state->policy_page : &state->claim_page;
    PageRange range = make_page_range(*page, total);
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
        handle_cancel_claim(ctx, state);
        break;
    case INS_REVIEW_CLAIM:
        handle_review_claim(ctx, state);
        break;
    case INS_SETTLE_CLAIM:
        handle_settle_claim(ctx, state);
        break;
    case INS_PREV:
        if (*page > 0)
            (*page)--;
        break;
    case INS_NEXT:
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

/* 用与绘制页面相同的矩形判断点击，并返回对应操作编号。 */
int hit_test_insurance(int x, int y)
{
    return hit_test_buttons(INSURANCE_BUTTONS,
                            (int)(sizeof(INSURANCE_BUTTONS) / sizeof(INSURANCE_BUTTONS[0])), x, y);
}
