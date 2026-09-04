#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

static const wchar_t* PRODUCT_NAMES_WIDE[] = { L"交强险", L"车损险", L"三者险" };
static const double PRODUCT_COVERAGE_RATIO[] = { 0.30, 0.80, 1.00 };
static const double PRODUCT_PREMIUM_MULT[] = { 0.8, 1.2, 1.5 };
static const int PRODUCT_COUNT = 3;

static void format_policy_line(const Policy* policy, wchar_t* line, size_t count)
{
    wchar_t policy_id[40], plate[32], owner[64], desc[96];
    char_to_wchar(policy->policy_id, policy_id, 40);
    char_to_wchar(policy->plate, plate, 32);
    char_to_wchar(policy->owner, owner, 64);
    char_to_wchar(policy->coverage_desc, desc, 96);
    _snwprintf_s(line, count, _TRUNCATE, L"%-10s %-8s %-10s %.2f %.2f %-8s %s", policy_id, plate, owner, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"失效", desc);
}

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

void draw_insurance(const AppContext* ctx, const GuiState* state, const Button* buttons, int count)
{
    int i;

    /* 在进入保单界面前更新保单状态，自动过期 */
    update_policy_active_status((AppContext*)ctx);

    draw_title(L"保单与理赔管理");
    draw_status(ctx);
    draw_message_box(state);
    for (i = 0; i < count; ++i) draw_button(&buttons[i]);
    draw_content_panel();

    if (state->insurance_view == VIEW_POLICY_LIST) {
        draw_table_header(L"保单号    车牌    投保人      保额     保费   状态   描述");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int indices[MAX_POLICIES] = { 0 };
        int total = collect_visible_policy_indices(ctx, indices, MAX_POLICIES);
        int page = state->policy_page;
        if (total == 0) { wcsncpy_s(buffer[0], 256, L"无保单记录", _TRUNCATE); lines[0] = buffer[0]; draw_rows(lines, 1); draw_footer_page(0, 0); return; }
        int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE; if (page >= total_pages) page = total_pages - 1; if (page < 0) page = 0; int start = page * PAGE_SIZE; int end = start + PAGE_SIZE; if (end > total) end = total; int row_count = 0; for (i = start; i < end; ++i) { const Policy* p = &ctx->policies[indices[i]]; format_policy_line(p, buffer[row_count], 256); lines[row_count] = buffer[row_count]; row_count++; } draw_rows(lines, row_count); draw_footer_page(page, total);
    } else {
        draw_table_header(L"理赔号    保单号    车牌    申请人    申请(元)  批准(元)  状态   描述");
        const wchar_t* lines[PAGE_SIZE] = { 0 };
        wchar_t buffer[PAGE_SIZE][256] = { 0 };
        int indices[MAX_CLAIMS] = { 0 };
        int total = collect_visible_claim_indices(ctx, indices, MAX_CLAIMS);
        int page = state->claim_page;
        if (total == 0) { wcsncpy_s(buffer[0], 256, L"无理赔记录", _TRUNCATE); lines[0] = buffer[0]; draw_rows(lines, 1); draw_footer_page(0, 0); return; }
        int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE; if (page >= total_pages) page = total_pages - 1; if (page < 0) page = 0; int start = page * PAGE_SIZE; int end = start + PAGE_SIZE; if (end > total) end = total; int row_count = 0; for (i = start; i < end; ++i) { const Claim* c = &ctx->claims[indices[i]]; format_claim_line(c, buffer[row_count], 256); lines[row_count] = buffer[row_count]; row_count++; } draw_rows(lines, row_count); draw_footer_page(page, total);
    }
}

static int prompt_product_choice(const wchar_t* title, int default_choice)
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
    RECT title_rect = { dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48 };
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // description
    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = { dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 84 };
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
    for (int i = 0; i < PRODUCT_COUNT; ++i) {
        btns[i].rect.left = start_x + i * (btn_w + spacing);
        btns[i].rect.top = btn_y;
        btns[i].rect.right = btns[i].rect.left + btn_w;
        btns[i].rect.bottom = btn_y + btn_h;
        btns[i].label = PRODUCT_NAMES_WIDE[i];
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

    // CHANGED: 高亮显示默认选项
    if (default_choice > 0 && default_choice <= PRODUCT_COUNT) {
        setfillcolor(RGB(230, 245, 255));
        setlinecolor(RGB(100, 140, 180));
        solidrectangle(btns[default_choice - 1].rect.left, btns[default_choice - 1].rect.top, btns[default_choice - 1].rect.right, btns[default_choice - 1].rect.bottom);
        settextcolor(RGB(20, 60, 100));
        settextstyle(18, 0, L"Segoe UI");
        draw_text_rect(btns[default_choice - 1].label, btns[default_choice - 1].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    FlushBatchDraw();

    // wait for click
    ExMessage msg;
    while (1) {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN) {
            int mx = msg.x;
            int my = msg.y;
            for (int i = 0; i < total_btn; ++i) {
                if (point_in_rect(mx, my, &btns[i].rect)) {
                    // small visual feedback: redraw pressed button (optional)
                    setlinecolor(RGB(80, 90, 110));
                    setfillcolor(RGB(220, 230, 240));
                    solidrectangle(btns[i].rect.left, btns[i].rect.top, btns[i].rect.right, btns[i].rect.bottom);
                    settextcolor(RGB(35, 40, 50));
                    settextstyle(18, 0, L"Segoe UI");
                    draw_text_rect(btns[i].label, btns[i].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    FlushBatchDraw();
                    // consume event and return id
                    return btns[i].id;
                }
            }
            // click outside: treat as cancel
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom) {
                return 0;
            }
        }
        Sleep(10);
    }
}

static void handle_add_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = {0}; char plate[10] = {0}; char owner[20] = {0};
    if (!prompt_char_field(L"添加保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return;
    if (!validate_string_len(policy_id, (int)sizeof(((Policy*)0)->policy_id))) { set_message(state, L"保单号长度不合法"); return; }
    if (find_policy(ctx, policy_id)) { set_message(state, L"保单号已存在"); return; }
    if (!prompt_char_field(L"添加保单", L"请输入投保车牌：", plate, sizeof(plate), "")) return;
    if (!validate_plate(plate)) { set_message(state, L"车牌格式不合法"); return; }
    Car* car = find_car(ctx, plate);
    if (!car) { set_message(state, L"未找到该车牌"); return; }
    if (is_admin(ctx)) {
        if (!prompt_char_field(L"添加保单", L"请输入投保人用户名：", owner, sizeof(owner), "")) return;
        if (!validate_string_len(owner, (int)sizeof(((User*)0)->username)) || !user_exists(ctx, owner)) { set_message(state, L"指定的用户不存在或用户名长度不合法"); return; }
    } else {
        strncpy(owner, ctx->current_user, sizeof(owner)-1); owner[sizeof(owner)-1] = '\0';
    }
    if (strcmp(owner, car->owner) != 0) { set_message(state, L"投保人必须为车辆所有者"); return; }

    // Use clickable product selection
    int choice = prompt_product_choice(L"选择保险产品", 1);
    if (choice <= 0 || choice > PRODUCT_COUNT) { set_message(state, L"已取消产品选择"); return; }

    Date start = today_local(); Date end = start; end.year = start.year + 1; if (!prompt_date_field(L"保单生效日", &start, &start)) return; if (!prompt_date_field(L"保单失效日", &end, &end)) return; if (!validate_date(&start) || !validate_date(&end) || compare_date_local(&start, &end) > 0) { set_message(state, L"生效/失效日期不合法"); return; }
    char product_desc[64] = {0}; wchar_to_char(PRODUCT_NAMES_WIDE[choice - 1], product_desc, sizeof(product_desc));
    if (add_policy(ctx, policy_id, plate, owner, product_desc) == 0) {
        Policy* p = find_policy(ctx, policy_id);
        if (p) {
            double ratio = PRODUCT_COVERAGE_RATIO[choice - 1];
            double mult = PRODUCT_PREMIUM_MULT[choice - 1];
            p->coverage_limit = car->purchase_price * ratio; if (p->coverage_limit <= 0.0) p->coverage_limit = 5000.0;
            p->premium = calculate_premium_for_car(car) * mult; p->start_date = start; p->end_date = end;
            Date now = today_local(); p->active = (compare_date_local(&p->start_date, &now) <= 0 && compare_date_local(&p->end_date, &now) >= 0) ? 1 : 0;
            strncpy(p->coverage_desc, product_desc, sizeof(p->coverage_desc)-1); p->coverage_desc[sizeof(p->coverage_desc)-1] = '\0';
            save_policies(ctx); append_operation_log(ctx, "ADD_POLICY", policy_id, product_desc); set_message(state, L"添加保单成功"); state->insurance_view = VIEW_POLICY_LIST; state->policy_page = 0;
        } else set_message(state, L"添加保单失败（查找新保单失败）");
    } else set_message(state, L"添加保单失败（可能已存在或达到上限）");
}

static void handle_delete_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = {0}; if (!prompt_char_field(L"删除保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return; Policy* p = find_policy(ctx, policy_id); if (!p) { set_message(state, L"未找到该保单"); return; } if (!is_admin(ctx) && strcmp(p->owner, ctx->current_user) != 0) { set_message(state, L"只能删除当前用户相关的保单"); return; } if (remove_policy(ctx, policy_id) == 0) { save_policies(ctx); append_operation_log(ctx, "DELETE_POLICY", policy_id, "deleted by user"); set_message(state, L"删除保单成功"); } else set_message(state, L"删除保单失败"); }

static void handle_find_policy(AppContext* ctx, GuiState* state)
{
    char policy_id[24] = {0}; if (!prompt_char_field(L"查询保单", L"请输入保单号：", policy_id, sizeof(policy_id), "")) return; const Policy* policy = find_visible_policy(ctx, policy_id); if (!policy) { set_message(state, L"未找到保单"); return; }
    wchar_t id_t[40], plate_t[32], owner_t[64], desc_t[96], message[512]; char_to_wchar(policy->policy_id, id_t, 40); char_to_wchar(policy->plate, plate_t, 32); char_to_wchar(policy->owner, owner_t, 64); char_to_wchar(policy->coverage_desc, desc_t, 96); _snwprintf_s(message, 512, _TRUNCATE, L"保单:%s 车牌:%s 投保人:%s 保额:%.2f 保费:%.2f 状态:%s 描述:%s", id_t, plate_t, owner_t, policy->coverage_limit, policy->premium, policy->active ? L"有效" : L"失效", desc_t); set_message(state, message);
}

static void handle_add_claim(AppContext* ctx, GuiState* state)
{
    char claim_id[24] = {0}, policy_id[24] = {0}, desc[256] = {0}, claimant[20] = {0}; double amount = 0.0;
    if (!prompt_char_field(L"新增理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), "")) return; if (!validate_string_len(claim_id, (int)sizeof(((Claim*)0)->claim_id))) { set_message(state, L"理赔单号长度不合法"); return; }
    if (!prompt_char_field(L"新增理赔", L"请输入关联保单号：", policy_id, sizeof(policy_id), "")) return; if (!validate_string_len(policy_id, (int)sizeof(((Policy*)0)->policy_id))) { set_message(state, L"保单号格式不合法"); return; }
    if (!prompt_double_field(L"新增理赔", L"请输入申请金额（元）：", &amount, 1000.0)) return; if (amount <= 0.0 || amount > 1e9) { set_message(state, L"申请金额不合法（必须大于0且小于1e9）"); return; }
    if (!prompt_char_field(L"新增理赔", L"请输入理赔说明：", desc, sizeof(desc), "")) return; if (!validate_string_len(desc, (int)sizeof(((Claim*)0)->description))) { set_message(state, L"理赔说明长度不合法"); return; }
    if (is_admin(ctx)) { if (!prompt_char_field(L"新增理赔", L"请输入申请人用户名：", claimant, sizeof(claimant), "")) return; if (!validate_string_len(claimant, (int)sizeof(((User*)0)->username)) || !user_exists(ctx, claimant)) { set_message(state, L"指定的用户不存在或用户名长度不合法"); return; } } else { strncpy(claimant, ctx->current_user, sizeof(claimant)-1); claimant[sizeof(claimant)-1] = '\0'; }
    if (add_claim_for_current_user(ctx, claim_id, policy_id, claimant, desc, amount)) { save_claims(ctx); append_operation_log(ctx, "ADD_CLAIM", claim_id, desc); set_message(state, L"提交理赔成功"); state->insurance_view = VIEW_CLAIM_LIST; state->claim_page = 0; } else set_message(state, L"提交理赔失败");
}

static void handle_settle_claim(AppContext* ctx, GuiState* state)
{
    char claim_id[24] = {0};
    if (!prompt_char_field(L"结案理赔", L"请输入理赔单号：", claim_id, sizeof(claim_id), "")) return;
    Claim* c = find_claim(ctx, claim_id);
    if (!c) {
        set_message(state, L"未找到理赔单");
        return;
    }
    if (!is_admin(ctx) && strcmp(c->claimant, ctx->current_user) != 0) {
        set_message(state, L"只能结案当前用户提交的理赔");
        return;
    }
    if (settle_claim_for_current_user(ctx, claim_id)) {
        save_claims(ctx);
        append_operation_log(ctx, "SETTLE_CLAIM", claim_id, "settled");
        set_message(state, L"理赔结案成功");
    } else {
        set_message(state, L"理赔结案失败");
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

int hit_test_insurance(int x, int y) { Button buttons[] = { {{20, 120, 200, 160}, L"", INS_ADD_POLICY}, {{20, 172, 200, 212}, L"", INS_DELETE_POLICY}, {{20, 224, 200, 264}, L"", INS_FIND_POLICY}, {{20, 276, 200, 316}, L"", INS_LIST_POLICIES}, {{20, 328, 200, 368}, L"", INS_ADD_CLAIM}, {{20, 380, 200, 420}, L"", INS_LIST_CLAIMS}, {{20, 432, 200, 472}, L"", INS_SETTLE_CLAIM}, {{20, 484, 95, 524}, L"", INS_PREV}, {{125, 484, 200, 524}, L"", INS_NEXT}, {{20, 544, 200, 584}, L"", INS_BACK} }; return hit_test_buttons(buttons, (int)(sizeof(buttons) / sizeof(buttons[0])), x, y); }
