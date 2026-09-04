#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

Date today_local(void)
{
    Date date;
    time_t now = time(NULL);
    struct tm local_time;
#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    date.year = local_time.tm_year + 1900;
    date.month = local_time.tm_mon + 1;
    date.day = local_time.tm_mday;
    return date;
}
void set_message(GuiState* state, const wchar_t* text)
{
    if (!state) return;
    if (text) {
        wcsncpy_s(state->message, 512, text, _TRUNCATE);
    } else {
        state->message[0] = L'\0';
    }
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

static int prompt_text(const wchar_t* title, const wchar_t* prompt, wchar_t* buffer, int count, const wchar_t* default_value)
{
    if (!buffer || count <= 0) return 0;
    buffer[0] = L'\0';
    /* InputBox 为 EasyX 提供的对话框函数，返回非0表示用户确认 */
    return InputBox(buffer, count, prompt, title, default_value ? default_value : L"");
}

int prompt_char_field(const wchar_t* title, const wchar_t* prompt, char* dest, int dest_count, const char* default_value)
{
    wchar_t text[256] = { 0 };
    wchar_t initial[256] = { 0 };
    if (default_value) char_to_wchar(default_value, initial, 256);
    if (!prompt_text(title, prompt, text, 256, initial)) return 0;
    if (text[0] == L'\0') return 0;
    wchar_to_char(text, dest, dest_count);
    return dest[0] != '\0';
}

int prompt_double_field(const wchar_t* title, const wchar_t* prompt, double* value, double default_value)
{
    wchar_t text[64] = { 0 };
    wchar_t initial[64] = { 0 };
    _snwprintf_s(initial, 64, _TRUNCATE, L"%.2f", default_value);
    if (!prompt_text(title, prompt, text, 64, initial)) return 0;
    if (text[0] == L'\0') return 0;
    *value = _wtof(text);
    return 1;
}

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

static int days_in_month(int year, int month)
{
    static const int mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month < 1 || month > 12) return 31;
    if (month == 2) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return 28 + leap;
    }
    return mdays[month - 1];
}

static void redraw_date_dialog(const wchar_t* title, int year, int month, int day,
    int dlg_left, int dlg_top, int dlg_right, int dlg_bottom, int dialog_w,
    int side_w, int gap_to_value, int gap_after_value, int spacing,
    int group_w, int small_group_w, int group_h, int y_top,
    int group_total1, int group_total2, int start_x, Button* btns)
{
        // background
        setlinecolor(RGB(120, 130, 150));
        setfillcolor(RGB(255,255,255));
        solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
        // title
        settextstyle(22,0,L"Segoe UI");
        settextcolor(RGB(35,40,50));
        RECT title_rect = { dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48 };
        draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        // description
        settextstyle(18,0,L"Segoe UI");
        RECT desc_rect = { dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 78 };
        draw_text_rect(L"请选择日期：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        // YEAR group
        int gx = start_x;
        int gy = y_top;
        // year minus
        btns[0].rect.left = gx;
        btns[0].rect.top = gy;
        btns[0].rect.right = gx + side_w;
        btns[0].rect.bottom = gy + group_h;
        btns[0].label = L"-";
        btns[0].id = 1;
        draw_button(&btns[0]);
        // year value box
        RECT yr_rect = { gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + group_w, gy + group_h };
        wchar_t tmp[64];
        _snwprintf_s(tmp, 64, _TRUNCATE, L"%04d", year);
        settextstyle(20,0,L"Segoe UI"); settextcolor(RGB(35,40,50));
        draw_text_rect(tmp, yr_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        // year plus
        btns[1].rect.left = yr_rect.right + gap_after_value;
        btns[1].rect.top = gy;
        btns[1].rect.right = btns[1].rect.left + side_w;
        btns[1].rect.bottom = gy + group_h;
        btns[1].label = L"+";
        btns[1].id = 2;
        draw_button(&btns[1]);

        // move to month group (CHANGED: 正确偏移以避免与 year plus 重叠)
        gx = gx + group_total1 + spacing;

        // MONTH group
        btns[2].rect.left = gx;
        btns[2].rect.top = gy;
        btns[2].rect.right = gx + side_w;
        btns[2].rect.bottom = gy + group_h;
        btns[2].label = L"-";
        btns[2].id = 3;
        draw_button(&btns[2]);
        RECT mo_rect = { gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + small_group_w, gy + group_h };
        _snwprintf_s(tmp, 64, _TRUNCATE, L"%02d", month);
        settextstyle(20,0,L"Segoe UI"); draw_text_rect(tmp, mo_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        btns[3].rect.left = mo_rect.right + gap_after_value;
        btns[3].rect.top = gy;
        btns[3].rect.right = btns[3].rect.left + side_w;
        btns[3].rect.bottom = gy + group_h;
        btns[3].label = L"+";
        btns[3].id = 4;
        draw_button(&btns[3]);

        // move to day group
        gx = gx + group_total2 + spacing;

        // DAY group
        btns[4].rect.left = gx;
        btns[4].rect.top = gy;
        btns[4].rect.right = gx + side_w;
        btns[4].rect.bottom = gy + group_h;
        btns[4].label = L"-";
        btns[4].id = 5;
        draw_button(&btns[4]);
        RECT day_rect = { gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + small_group_w, gy + group_h };
        _snwprintf_s(tmp, 64, _TRUNCATE, L"%02d", day);
        settextstyle(20,0,L"Segoe UI"); draw_text_rect(tmp, day_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        btns[5].rect.left = day_rect.right + gap_after_value;
        btns[5].rect.top = gy;
        btns[5].rect.right = btns[5].rect.left + side_w;
        btns[5].rect.bottom = gy + group_h;
        btns[5].label = L"+";
        btns[5].id = 6;
        draw_button(&btns[5]);

        // confirm / cancel
        int bw = 120; int bh = 44;
        btns[6].rect.left = dlg_left + dialog_w - bw - 36; btns[6].rect.top = dlg_bottom - bh - 20; btns[6].rect.right = btns[6].rect.left + bw; btns[6].rect.bottom = btns[6].rect.top + bh; btns[6].label = L"确定"; btns[6].id = 7; draw_button(&btns[6]);
        btns[7].rect.left = btns[6].rect.left - bw - 12; btns[7].rect.top = btns[6].rect.top; btns[7].rect.right = btns[7].rect.left + bw; btns[7].rect.bottom = btns[7].rect.top + bh; btns[7].label = L"取消"; btns[7].id = 0; draw_button(&btns[7]);

        FlushBatchDraw();
}
int prompt_date_field(const wchar_t* title, Date* date, const Date* current)
{
    if (!date) return 0;
    int year, month, day;
    if (current) {
        year = current->year;
        month = current->month;
        day = current->day;
    } else {
        time_t t = time(NULL);
        struct tm tmv;
    #ifdef _WIN32
        localtime_s(&tmv, &t);
    #else
        localtime_r(&t, &tmv);
    #endif
        year = tmv.tm_year + 1900;
        month = tmv.tm_mon + 1;
        day = tmv.tm_mday;
    }
    if (year < 1900) year = 1900; if (year > 3000) year = 3000;
    if (month < 1) month = 1; if (month > 12) month = 12;
    int dim = days_in_month(year, month);
    if (day < 1) day = 1; if (day > dim) day = dim;

    int dialog_w = 520;
    int dialog_h = 220;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    // layout 参数（CHANGED: 用明确的 side/gap 值并按组计算总宽，避免重叠）
    const int side_w = 36;           // 单侧按钮宽度（+/-）
    const int gap_to_value = 6;      // 侧钮到数值框的空隙
    const int gap_after_value = 8;   // 数值框到侧钮的空隙
    const int spacing = 18;          // 组间间隔
    const int group_w = 120;         // 年组数值框宽
    const int small_group_w = group_w / 2; // 月/日组数值框宽
    const int group_h = 48;
    int y_top = dlg_top + 70;

    // 计算三组总宽并居中（CHANGED）
    int group_total1 = side_w + gap_to_value + group_w + gap_after_value + side_w;
    int group_total2 = side_w + gap_to_value + small_group_w + gap_after_value + side_w;
    int group_total3 = group_total2;
    int groups_total = group_total1 + group_total2 + group_total3 + spacing * 2;
    int start_x = dlg_left + (dialog_w - groups_total) / 2;

    // button areas
    Button btns[8]; // year-, year+, month-, month+, day-, day+, confirm, cancel
    for (int i = 0; i < 8; ++i) { btns[i].id = 0; btns[i].label = L""; btns[i].rect.left = btns[i].rect.top = btns[i].rect.right = btns[i].rect.bottom = 0; }

    // draw helper（使用按组布局避免重叠，CHANGED）


    // 初次绘制
    redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns);

    ExMessage msg;
    while (1) {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN) {
            int mx = msg.x, my = msg.y;
            // check buttons
            for (int i = 0; i < 8; ++i) {
                if (btns[i].id == 0) continue; // skip empty / cancel uses id 0 so skip here, will handle separately
                if (point_in_rect(mx, my, &btns[i].rect)) {
                    int id = btns[i].id;
                    if (id == 1) { // year -
                        if (year > 1900) year--; if (year < 1900) year = 1900; int dim2 = days_in_month(year, month); if (day > dim2) day = dim2; redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 2) { // year +
                        if (year < 3000) year++; if (year > 3000) year = 3000; int dim2 = days_in_month(year, month); if (day > dim2) day = dim2; redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 3) { // month -
                        month = month > 1 ? month - 1 : 12; int dim2 = days_in_month(year, month); if (day > dim2) day = dim2; redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 4) { // month +
                        month = month < 12 ? month + 1 : 1; int dim2 = days_in_month(year, month); if (day > dim2) day = dim2; redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 5) { // day -
                        day = day > 1 ? day - 1 : days_in_month(year, month); redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 6) { // day +
                        int dim2 = days_in_month(year, month); day = day < dim2 ? day + 1 : 1; redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w, side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w, group_h, y_top, group_total1, group_total2, start_x, btns); continue;
                    } else if (id == 7) { // confirm
                        date->year = year; date->month = month; date->day = day; return 1;
                    }
                }
            }
            // 处理取消按钮（id == 0）或点击对话框外部视为取消
            if (point_in_rect(mx, my, &btns[7].rect)) return 0;
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom) return 0;
        }
        Sleep(10);
    }
}

const wchar_t* violation_to_text(ViolationLevel level)
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

int prompt_violation_field(const wchar_t* title, ViolationLevel* level, ViolationLevel current)
{
    const wchar_t* names[] = { L"无", L"较轻微", L"轻微", L"中等", L"较严重", L"严重" };
    const int count = 6;

    // dialog dimensions
    int dialog_w = 480;
    int dialog_h = 200;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    // background + title
    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
    settextstyle(22, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    RECT title_rect = { dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48 };
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = { dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 84 };
    // CHANGED: 文案使用“违章记录”以与表头一致
    draw_text_rect(L"请选择违章记录：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Two-row layout: first row up to 4 options, second row remaining + cancel
    int max_first = 4;
    int first_count = (count < max_first) ? count : max_first;
    int remaining = count - first_count; // options placed in second row

    int btn_w = 110;
    int btn_h = 40;
    int spacing = 12;

    // positions
    int row1_total_w = first_count * btn_w + (first_count - 1) * spacing;
    int start_x_row1 = dlg_left + (dialog_w - row1_total_w) / 2;
    int row1_y = dlg_top + 100; // upper row

    int second_elements = (remaining > 0 ? remaining : 0) + 1; // remaining options + cancel
    int row2_total_w = second_elements * btn_w + (second_elements - 1) * spacing;
    int start_x_row2 = dlg_left + (dialog_w - row2_total_w) / 2;
    int row2_y = dlg_bottom - btn_h - 20; // lower row

    Button btns[8];
    int idx = 0;

    // first row
    for (int i = 0; i < first_count; ++i, ++idx) {
        btns[idx].rect.left = start_x_row1 + i * (btn_w + spacing);
        btns[idx].rect.top = row1_y;
        btns[idx].rect.right = btns[idx].rect.left + btn_w;
        btns[idx].rect.bottom = row1_y + btn_h;
        btns[idx].label = names[idx];
        btns[idx].id = idx;
        if (idx == (int)current) {
            setfillcolor(RGB(230, 245, 255));
            setlinecolor(RGB(100, 140, 180));
            solidrectangle(btns[idx].rect.left, btns[idx].rect.top, btns[idx].rect.right, btns[idx].rect.bottom);
            settextcolor(RGB(20, 60, 100));
            settextstyle(18, 0, L"Segoe UI");
            draw_text_rect(btns[idx].label, btns[idx].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            draw_button(&btns[idx]);
        }
    }

    // second row: remaining options
    for (int j = 0; j < remaining; ++j, ++idx) {
        int opt = first_count + j;
        btns[idx].rect.left = start_x_row2 + j * (btn_w + spacing);
        btns[idx].rect.top = row2_y;
        btns[idx].rect.right = btns[idx].rect.left + btn_w;
        btns[idx].rect.bottom = row2_y + btn_h;
        btns[idx].label = names[opt];
        btns[idx].id = opt;
        if (opt == (int)current) {
            setfillcolor(RGB(230, 245, 255));
            setlinecolor(RGB(100, 140, 180));
            solidrectangle(btns[idx].rect.left, btns[idx].rect.top, btns[idx].rect.right, btns[idx].rect.bottom);
            settextcolor(RGB(20, 60, 100));
            settextstyle(18, 0, L"Segoe UI");
            draw_text_rect(btns[idx].label, btns[idx].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            draw_button(&btns[idx]);
        }
    }

    // cancel on second row at the end
    btns[idx].rect.left = start_x_row2 + (second_elements - 1) * (btn_w + spacing);
    btns[idx].rect.top = row2_y;
    btns[idx].rect.right = btns[idx].rect.left + btn_w;
    btns[idx].rect.bottom = row2_y + btn_h;
    btns[idx].label = L"取消";
    btns[idx].id = -1;
    draw_button(&btns[idx]);
    ++idx;

    FlushBatchDraw();

    ExMessage msg;
    while (1) {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN) {
            int mx = msg.x;
            int my = msg.y;
            for (int i = 0; i < idx; ++i) {
                if (point_in_rect(mx, my, &btns[i].rect)) {
                    if (btns[i].id >= 0 && btns[i].id < count) {
                        *level = (ViolationLevel)btns[i].id;
                        return 1;
                    }
                    return 0; // cancel
                }
            }
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom) return 0;
        }
        Sleep(10);
    }
}

int is_logged_in(const AppContext* ctx)
{
    return app_is_logged_in(ctx);
}

int is_admin(const AppContext* ctx)
{
    return app_is_admin(ctx);
}

int point_in_rect(int x, int y, const RECT* rect)
{
    return x >= rect->left && x <= rect->right && y >= rect->top && y <= rect->bottom;
}
static void fill_rect_color(int left, int top, int right, int bottom, COLORREF line, COLORREF fill)
{
    setlinecolor(line);
    setfillcolor(fill);
    solidrectangle(left, top, right, bottom);
}

void draw_text_rect(const wchar_t* text, RECT rect, UINT format)
{
    drawtext(text, &rect, format);
}

void draw_button(const Button* button)
{
    fill_rect_color(button->rect.left, button->rect.top, button->rect.right, button->rect.bottom, RGB(210, 220, 235), RGB(245, 248, 252));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(button->label, button->rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
    if (is_logged_in(ctx)) _snwprintf_s(status, 128, _TRUNCATE, L"当前用户：%s", user);
    else wcsncpy_s(status, 128, L"当前用户：未登录", _TRUNCATE);
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

void draw_content_panel(void)
{
    fill_rect_color(CONTENT_LEFT, CONTENT_TOP, CONTENT_RIGHT, CONTENT_BOTTOM, RGB(220, 228, 238), RGB(255, 255, 255));
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

void draw_footer_page(int page, int total_items)
{
    int total_pages = total_items == 0 ? 1 : (total_items + PAGE_SIZE - 1) / PAGE_SIZE;
    wchar_t page_text[64] = { 0 };
    _snwprintf_s(page_text, 64, _TRUNCATE, L"第 %d / %d 页，共 %d 条", page + 1, total_pages, total_items);
    RECT rect = { CONTENT_LEFT + 20, CONTENT_BOTTOM - 40, CONTENT_RIGHT - 150, CONTENT_BOTTOM - 10 };
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(90, 100, 120));
    draw_text_rect(page_text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

int hit_test_buttons(const Button* buttons, int count, int x, int y)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (point_in_rect(x, y, &buttons[i].rect)) return buttons[i].id;
    }
    return 0;
}

int compare_date_local(const Date* a, const Date* b)
{
    if (!a || !b) return 0;
    if (a->year != b->year) return (a->year < b->year) ? -1 : 1;
    if (a->month != b->month) return (a->month < b->month) ? -1 : 1;
    if (a->day != b->day) return (a->day < b->day) ? -1 : 1;
    return 0;
}

static int is_alnum_string(const char* s)
{
    if (!s || s[0] == '\0') return 0;
    for (const char* p = s; *p; ++p) {
        if (!(((*p) >= '0' && (*p) <= '9') || ((*p) >= 'A' && (*p) <= 'Z') || ((*p) >= 'a' && (*p) <= 'z') || *p == '-')) return 0;
    }
    return 1;
}

int validate_plate(const char* plate)
{
    if (!plate || plate[0] == '\0') return 0;
    size_t len = strlen(plate);
    if (len < 1 || len >= sizeof(((Car*)0)->plate)) return 0; // ensure fits Car.plate
    if (!is_alnum_string(plate)) return 0;
    return 1;
}

int validate_string_len(const char* s, int maxlen)
{
    if (!s) return 0;
    size_t len = strlen(s);
    return len > 0 && len < (size_t)maxlen;
}

int validate_price_value(double v)
{
    if (v < 0.0) return 0;
    if (v > 1e9) return 0;
    return 1;
}

int validate_date(const Date* d)
{
    if (!d) return 0;
    if (d->year < 1900 || d->year > 3000) return 0;
    if (d->month < 1 || d->month > 12) return 0;
    if (d->day < 1 || d->day > 31) return 0;
    return 1;
}
