#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

/*
 * gui_common.c —— GUI 共用的输入、字符转换、绘图、命中检测和验证工具
 *
 * Windows/EasyX 的中文界面主要使用 wchar_t 宽字符串，业务结构体和文本
 * 文件使用 char 字符串，因此本文件位于二者之间，负责转换和通用界面逻辑。
 */

/* 安全更新底部消息；text 为 NULL 时清空。_TRUNCATE 表示过长就截断。 */
void set_message(GuiState *state, const wchar_t *text)
{
    /* state 为空时没有可写目标，立即返回可避免访问 state->message 崩溃。 */
    if (!state)
        return;
    if (text)
    {
        /* wcsncpy_s 是带容量的宽字符串复制；_TRUNCATE 允许安全截断长提示。 */
        wcsncpy_s(state->message, 512, text, _TRUNCATE);
    }
    else
    {
        state->message[0] = L'\0';
    }
}

/*
 * 绘制确认框中的一个按钮。primary 为真时绘制实心强调按钮；否则绘制浅色
 * 取消按钮。dangerous 只影响主按钮颜色，删除类操作使用红色提醒用户。
 */
static void draw_confirm_button(const RECT *rect, const wchar_t *label, int primary,
                                int dangerous)
{
    COLORREF fill;
    COLORREF border;
    COLORREF text;
    RECT text_rect;

    /* primary 决定“确认/取消”的视觉层级；dangerous 只在确认按钮上把蓝色
       换成红色。这里仅影响绘制，不改变点击后的返回值。 */
    if (primary)//红蓝，浅
    {
        fill = dangerous ? RGB(220, 70, 70) : RGB(59, 130, 246);
        border = dangerous ? RGB(190, 55, 55) : RGB(37, 99, 235);
        text = RGB(255, 255, 255);
    }
    else
    {
        fill = RGB(241, 245, 249);
        border = RGB(203, 213, 225);
        text = RGB(51, 65, 85);
    }

    setlinecolor(border);
    setfillcolor(fill);
    solidroundrect(rect->left, rect->top, rect->right, rect->bottom, 12, 12);
    setbkmode(TRANSPARENT);
    settextstyle(17, 0, L"Microsoft YaHei UI");
    settextcolor(text);
    /* RECT 是普通结构体，赋值会复制四个边界值；draw_text_rect 不会改原 rect。 */
    text_rect = *rect;
    draw_text_rect(label, text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/*
 * 统一 EasyX 确认框。
 *
 * 函数先用灰蓝色覆盖当前画面，形成遮罩效果，再在窗口中央绘制白色面板。
 * 随后的 while(1) 是一个临时模态事件循环：用户作出选择前，底层页面不会
 * 收到点击。函数返回后，GUI 主循环会重新绘制当前页面，因此不需要手工恢复
 * 被遮罩覆盖的画面。
 */
int show_confirm_dialog(const wchar_t *title, const wchar_t *message,
	const wchar_t* confirm_label, int dangerous)//参数为内容，危险操作为红色
{
    /* 先由固定宽高计算中心位置；后面的所有文字和按钮坐标均相对此面板。 */
    const int dialog_width = 560;
    const int dialog_height = 360;
    const int left = (WINDOW_WIDTH - dialog_width) / 2;
    const int top = (WINDOW_HEIGHT - dialog_height) / 2;
    const int right = left + dialog_width;
    const int bottom = top + dialog_height;
    RECT title_rect;
    RECT message_rect;
    RECT cancel_rect;
    RECT confirm_rect;
    ExMessage event;

    /* 调用者允许把三个文字参数传 NULL，此处替换为可显示的默认宽字符串。 */
    if (!title)
        title = L"请确认操作";
    if (!message)
        message = L"是否继续执行当前操作？";
    if (!confirm_label)
        confirm_label = dangerous ? L"确认删除" : L"确认";

    /* EasyX 没有透明遮罩控件，这里用统一的浅灰蓝色模拟背景变暗。 */
    setlinecolor(RGB(211, 219, 230));
    setfillcolor(RGB(211, 219, 230));
    solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* 先绘制偏移的阴影，再绘制弹窗主体。 */
    setlinecolor(RGB(174, 185, 201));
    setfillcolor(RGB(174, 185, 201));
    solidroundrect(left + 5, top + 6, right + 5, bottom + 6, 18, 18);
    setlinecolor(RGB(203, 213, 225));
    setfillcolor(RGB(255, 255, 255));
    solidroundrect(left, top, right, bottom, 18, 18);

    /* 左侧色条让普通确认和危险确认可以快速区分。 */
    setlinecolor(dangerous ? RGB(220, 70, 70) : RGB(59, 130, 246));
    setfillcolor(dangerous ? RGB(220, 70, 70) : RGB(59, 130, 246));
    solidroundrect(left + 22, top + 24, left + 30, top + 66, 6, 6);

    title_rect.left = left + 46;
    title_rect.top = top + 18;
    title_rect.right = right - 24;
    title_rect.bottom = top + 72;
    setbkmode(TRANSPARENT);
    settextstyle(25, 0, L"Microsoft YaHei UI");
    settextcolor(RGB(30, 41, 59));
    draw_text_rect(title, title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);//常规title

    setlinecolor(RGB(226, 232, 240));
    line(left + 24, top + 82, right - 24, top + 82);

    message_rect.left = left + 34;
    message_rect.top = top + 100;
    message_rect.right = right - 34;
    message_rect.bottom = bottom - 88;
    settextstyle(16, 0, L"Microsoft YaHei UI");
    settextcolor(dangerous ? RGB(127, 29, 29) : RGB(51, 65, 85));
    draw_text_rect(message, message_rect, DT_LEFT | DT_TOP | DT_WORDBREAK);//常规message

    cancel_rect.left = right - 276;
    cancel_rect.top = bottom - 66;
    cancel_rect.right = right - 154;
    cancel_rect.bottom = bottom - 22;
    confirm_rect.left = right - 142;
    confirm_rect.top = bottom - 66;
    confirm_rect.right = right - 22;
    confirm_rect.bottom = bottom - 22;
    draw_confirm_button(&cancel_rect, L"取消", 0, dangerous);
    draw_confirm_button(&confirm_rect, confirm_label, 1, dangerous);
    FlushBatchDraw();

    /* 模态事件循环只接受本对话框点击：确认返回 1，取消或点框外返回 0。 */
    while (1)
    {
        if (peekmessage(&event, EM_MOUSE, 1) && event.message == WM_LBUTTONDOWN)
        {
            if (point_in_rect(event.x, event.y, &confirm_rect))
                return 1;
            if (point_in_rect(event.x, event.y, &cancel_rect))
                return 0;
            /* 点击面板外部等价于取消，防止用户误以为弹窗已确认。 */
            if (event.x < left || event.x > right || event.y < top || event.y > bottom)
                return 0;
        }
        /* 没有消息时暂停 10ms，避免持续轮询占满 CPU。 */
        Sleep(10);
    }
}

/*
 * 将系统代码页的 char 字符串转换为 wchar_t 字符串。dest_count 是目标数组
 * 的元素数量而不是字节数，最后手工补 L'\0' 保证字符串结束。
 */
void char_to_wchar(const char *src, wchar_t *dest, int dest_count)
{
    /* 目标无效时无法报告错误，因此直接返回；调用者应始终传真实数组容量。 */
    if (!dest || dest_count <= 0)
        return;
    if (!src)
    {
        dest[0] = L'\0';
        return;
    }
    /* CP_ACP 使用当前 Windows 系统代码页；-1 表示连同源字符串末尾 '\0'
       一起转换。最后一行是额外保险，保证目标一定终止。 */
    MultiByteToWideChar(CP_ACP, 0, src, -1, dest, dest_count);
    dest[dest_count - 1] = L'\0';
}

/* 与上一个函数方向相反，把 GUI 宽字符串转回业务层 char 字符串。 */
void wchar_to_char(const wchar_t *src, char *dest, int dest_count)
{
    if (!dest || dest_count <= 0)
        return;
    if (!src)
    {
        dest[0] = '\0';
        return;
    }
    WideCharToMultiByte(CP_ACP, 0, src, -1, dest, dest_count, NULL, NULL);
    dest[dest_count - 1] = '\0';
}

/*
 * EasyX InputBox 的内部统一入口。
 *
 * title 是窗口标题，prompt 是问题文字，buffer/count 是接收输入的宽字符数组
 * 及其容量，default_value 是输入框初始内容。成功确认返回非 0，取消返回 0。
 * static 表示只供本文件的各种 prompt_* 函数复用，不暴露给其他模块。
 */
static int prompt_text(const wchar_t *title, const wchar_t *prompt, wchar_t *buffer, int count,
                       const wchar_t *default_value)
{
    /* InputBox 要向 buffer 写数据，所以先检查地址和容量，避免无效写入。 */
    if (!buffer || count <= 0)
        return 0;
    /* 即使 InputBox 失败，调用者也只会看到空字符串而不是旧内存内容。 */
    buffer[0] = L'\0';
    /* ?: 在 default_value 为 NULL 时改传空宽字符串，避免把空指针交给 EasyX。 */
    return InputBox(buffer, count, prompt, title, default_value ? default_value : L"");
}

/*
 * 收集一个必填的 char 字符串。
 *
 * EasyX 只直接处理 wchar_t，所以先用 text 接收宽字符，再转换到业务层的
 * dest。dest_count 是调用者目标数组容量。返回 1 表示得到了非空结果；取消、
 * 空输入或转换后为空都返回 0。函数不负责用户名等具体业务规则校验。
 */
int prompt_char_field(const wchar_t *title, const wchar_t *prompt, char *dest, int dest_count,
                      const char *default_value)
{
    /* text 接收新输入，initial 保存默认值；分开可避免 InputBox 覆盖默认值。 */
    wchar_t text[256] = {0};
    wchar_t initial[256] = {0};
    /* 业务层默认值是 char，显示前要转成 EasyX 使用的 wchar_t。 */
    if (default_value)
        char_to_wchar(default_value, initial, 256);
    /* ! 把“确认时的非 0”取反成假，把“取消时的 0”取反成真并提前返回。 */
    if (!prompt_text(title, prompt, text, 256, initial))
        return 0;
    /* 本接口代表必填字段，所以用户确认一个空字符串也视为没有有效输入。 */
    if (text[0] == L'\0')
        return 0;
    /* 转换函数会遵守 dest_count，并在最后补 '\0'。 */
    wchar_to_char(text, dest, dest_count);
    return dest[0] != '\0';
}

/*
 * 收集可选文本。这里不把空字符串当成取消：按“确定”后即使没有输入内容，
 * 仍返回 1，并把 dest 设为空字符串。查询功能由此表达“这个字段不限”。
 */
int prompt_optional_char_field(const wchar_t *title, const wchar_t *prompt, char *dest,
                               int dest_count, const char *default_value)
{
    wchar_t text[256] = {0};
    wchar_t initial[256] = {0};

    if (!dest || dest_count <= 0)
        return 0;
    if (default_value)
        char_to_wchar(default_value, initial, 256);
    if (!prompt_text(title, prompt, text, 256, initial))
        return 0;

    wchar_to_char(text, dest, dest_count);
    return 1;
}

/*
 * 收集浮点数。value 是输出指针，*value 表示修改调用者的实际变量；
 *
 * _wtof 把宽字符串转换为 double。
 */
int prompt_double_field(const wchar_t *title, const wchar_t *prompt, double *value,
                        double default_value)
{
    wchar_t text[64] = {0};
    wchar_t initial[64] = {0};
    _snwprintf_s(initial, 64, _TRUNCATE, L"%.2f", default_value);
    if (!prompt_text(title, prompt, text, 64, initial))
        return 0;
    if (text[0] == L'\0')
        return 0;
    *value = _wtof(text);
    return 1;
}

/*
 * 根据当前年月日重新绘制日期选择框，并把八个按钮的矩形/编号写入 btns。
 * 参数较多是因为所有布局计算结果都由 prompt_date_field 传入复用。
 */
static void redraw_date_dialog(const wchar_t *title, int year, int month, int day, int dlg_left,
                               int dlg_top, int dlg_right, int dlg_bottom, int dialog_w, int side_w,
                               int gap_to_value, int gap_after_value, int spacing, int group_w,
                               int small_group_w, int group_h, int y_top, int group_total1,
                               int group_total2, int start_x, Button *btns)
{
    /* 先重画对话框背景；日期改变后函数会再次调用，所以旧数字会被覆盖。 */
    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
    /* 标题区域。 */
    settextstyle(22, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    RECT title_rect = {dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48};
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    /* 操作说明区域。 */
    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = {dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 78};
    draw_text_rect(L"请选择日期：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    /* 年份组从 start_x 开始，由“减号、数值、加号”三个区域组成。 */
    int gx = start_x;
    int gy = y_top;
    /* 年份减号按钮，id=1 供事件循环识别。 */
    btns[0].rect.left = gx;
    btns[0].rect.top = gy;
    btns[0].rect.right = gx + side_w;
    btns[0].rect.bottom = gy + group_h;
    btns[0].label = L"-";
    btns[0].id = 1;
    draw_button(&btns[0]);
    /* 年份数值不是按钮，只负责显示当前 year。 */
    RECT yr_rect = {gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + group_w,
                    gy + group_h};
    wchar_t tmp[64];
    _snwprintf_s(tmp, 64, _TRUNCATE, L"%04d", year);
    settextstyle(20, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(tmp, yr_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    /* 年份加号按钮，id=2。 */
    btns[1].rect.left = yr_rect.right + gap_after_value;
    btns[1].rect.top = gy;
    btns[1].rect.right = btns[1].rect.left + side_w;
    btns[1].rect.bottom = gy + group_h;
    btns[1].label = L"+";
    btns[1].id = 2;
    draw_button(&btns[1]);

    /* 横坐标越过完整年份组，再留 spacing 像素进入月份组。 */
    gx = gx + group_total1 + spacing;

    /* 月份组使用 id=3 和 id=4 分别表示减少与增加。 */
    btns[2].rect.left = gx;
    btns[2].rect.top = gy;
    btns[2].rect.right = gx + side_w;
    btns[2].rect.bottom = gy + group_h;
    btns[2].label = L"-";
    btns[2].id = 3;
    draw_button(&btns[2]);
    RECT mo_rect = {gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + small_group_w,
                    gy + group_h};
    _snwprintf_s(tmp, 64, _TRUNCATE, L"%02d", month);
    settextstyle(20, 0, L"Segoe UI");
    draw_text_rect(tmp, mo_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    btns[3].rect.left = mo_rect.right + gap_after_value;
    btns[3].rect.top = gy;
    btns[3].rect.right = btns[3].rect.left + side_w;
    btns[3].rect.bottom = gy + group_h;
    btns[3].label = L"+";
    btns[3].id = 4;
    draw_button(&btns[3]);

    /* 横坐标继续移动到日期组。 */
    gx = gx + group_total2 + spacing;

    /* 日期组使用 id=5 和 id=6。 */
    btns[4].rect.left = gx;
    btns[4].rect.top = gy;
    btns[4].rect.right = gx + side_w;
    btns[4].rect.bottom = gy + group_h;
    btns[4].label = L"-";
    btns[4].id = 5;
    draw_button(&btns[4]);
    RECT day_rect = {gx + side_w + gap_to_value, gy, gx + side_w + gap_to_value + small_group_w,
                     gy + group_h};
    _snwprintf_s(tmp, 64, _TRUNCATE, L"%02d", day);
    settextstyle(20, 0, L"Segoe UI");
    draw_text_rect(tmp, day_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    btns[5].rect.left = day_rect.right + gap_after_value;
    btns[5].rect.top = gy;
    btns[5].rect.right = btns[5].rect.left + side_w;
    btns[5].rect.bottom = gy + group_h;
    btns[5].label = L"+";
    btns[5].id = 6;
    draw_button(&btns[5]);

    /* 确定 id=7；取消 id=0，事件循环用这两个值区分提交和放弃。 */
    int bw = 120;
    int bh = 44;
    btns[6].rect.left = dlg_left + dialog_w - bw - 36;
    btns[6].rect.top = dlg_bottom - bh - 20;
    btns[6].rect.right = btns[6].rect.left + bw;
    btns[6].rect.bottom = btns[6].rect.top + bh;
    btns[6].label = L"确定";
    btns[6].id = 7;
    draw_button(&btns[6]);
    btns[7].rect.left = btns[6].rect.left - bw - 12;
    btns[7].rect.top = btns[6].rect.top;
    btns[7].rect.right = btns[7].rect.left + bw;
    btns[7].rect.bottom = btns[7].rect.top + bh;
    btns[7].label = L"取消";
    btns[7].id = 0;
    draw_button(&btns[7]);

    FlushBatchDraw();
}
/*
 * 显示模态日期选择器。current 非 NULL 时作为初始日期，否则用今天。
 *
 * 内层 while(1) 持续等待点击；确定后通过 date 指针写回并返回 1，取消返回
 * 0。

 */
/* 把日期限制在 [min_date, max_date] 内；任一界为 NULL 表示不限制该侧。 */
static void clamp_date_range(int *year, int *month, int *day, const Date *min_date,
                             const Date *max_date)
{
    /* year/month/day 是三个输出指针。先组合临时 Date 便于复用统一比较函数；
       超界时再通过 *year 等写回调用者的实际变量。 */
    Date d;
    d.year = *year;
    d.month = *month;
    d.day = *day;
    if (min_date && date_compare(&d, min_date) < 0)
    {
        *year = min_date->year;
        *month = min_date->month;
        *day = min_date->day;
    }
    else if (max_date && date_compare(&d, max_date) > 0)
    {
        *year = max_date->year;
        *month = max_date->month;
        *day = max_date->day;
    }
}

int prompt_date_field_bounded(const wchar_t *title, Date *date, const Date *current,
                              const Date *min_date, const Date *max_date)
{
    /* date 是最终输出地址，NULL 时无法写回选择结果。 */
    if (!date)
        return 0;
    int year, month, day;
    /* 有 current 就复制为初始选择；否则把系统 struct tm 转为正常公历年月日。 */
    if (current)
    {
        year = current->year;
        month = current->month;
        day = current->day;
    }
    else
    {
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
    if (year < 1900)
        year = 1900;
    if (year > 3000)
        year = 3000;
    if (month < 1)
        month = 1;
    if (month > 12)
        month = 12;
    /* 年月修正后再限制日，尤其处理 2 月和大小月。 */
    int dim = date_days_in_month(year, month);
    if (day < 1)
        day = 1;
    if (day > dim)
        day = dim;//到此为修正
    clamp_date_range(&year, &month, &day, min_date, max_date);

    int dialog_w = 520;
    int dialog_h = 220;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    /* 日期按钮组布局：每一组由减号、数值框、加号组成。 */
    const int side_w = 36;                 /* 单侧加减按钮宽度。 */
    const int gap_to_value = 6;            /* 减号到数值框的空隙。 */
    const int gap_after_value = 8;         /* 数值框到加号的空隙。 */
    const int spacing = 18;                /* 年、月、日三组之间的间隔。 */
    const int group_w = 120;               /* 年份数值框宽度。 */
    const int small_group_w = group_w / 2; /* 月、日数值框宽度。 */
    const int group_h = 48;
    int y_top = dlg_top + 70;

    /* 计算年、月、日三组控件总宽度，再把整组控件水平居中。 */
    int group_total1 = side_w + gap_to_value + group_w + gap_after_value + side_w;
    int group_total2 = side_w + gap_to_value + small_group_w + gap_after_value + side_w;
    int group_total3 = group_total2;
    int groups_total = group_total1 + group_total2 + group_total3 + spacing * 2;
    int start_x = dlg_left + (dialog_w - groups_total) / 2;

    /* 八个按钮依次为年-/年+/月-/月+/日-/日+/确定/取消。 */
    Button btns[8];
    for (int i = 0; i < 8; ++i)
    {
        btns[i].id = 0;
        btns[i].label = L"";
        btns[i].rect.left = btns[i].rect.top = btns[i].rect.right = btns[i].rect.bottom = 0;
    }

    /* 首次绘制与每次数字变化后的重绘共用同一函数，避免坐标计算不一致。 */
    redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right, dlg_bottom, dialog_w,
                       side_w, gap_to_value, gap_after_value, spacing, group_w, small_group_w,
                       group_h, y_top, group_total1, group_total2, start_x, btns);//会给id

    ExMessage msg;
    while (1)
    {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN)
        {
            int mx = msg.x, my = msg.y;
            /* 顺序检查八个按钮；id 为 1~7 的按钮在此处理，取消按钮单独处理。 */
            for (int i = 0; i < 8; ++i)
            {
                if (btns[i].id == 0)
                    continue; /* id=0 是取消，在循环后单独判断其矩形。 */
                if (point_in_rect(mx, my, &btns[i].rect))
                {
                    int id = btns[i].id;
                    if (id == 1)
                    { /* 年减一 */
                        if (year > 1900)
                            year--;
                        if (year < 1900)
                            year = 1900;
                        int dim2 = date_days_in_month(year, month);
                        if (day > dim2)
                            day = dim2;
						clamp_date_range(&year, &month, &day, min_date, max_date);//这里调用一次，直接把 year month day 限制在范围内
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 2)
                    { /* 年加一 */
                        if (year < 3000)
                            year++;
                        if (year > 3000)
                            year = 3000;
                        int dim2 = date_days_in_month(year, month);
                        if (day > dim2)
                            day = dim2;
                        clamp_date_range(&year, &month, &day, min_date, max_date);
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 3)
                    { /* 月减一；1 月再减会循环到 12 月。 */
                        month = month > 1 ? month - 1 : 12;
                        int dim2 = date_days_in_month(year, month);
                        if (day > dim2)
                            day = dim2;
                        clamp_date_range(&year, &month, &day, min_date, max_date);
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 4)
                    { /* 月加一；12 月再加会循环到 1 月。 */
                        month = month < 12 ?       month + 1 : 1;
                        int dim2 = date_days_in_month(year, month);
                        if (day > dim2)
                            day = dim2;
                        clamp_date_range(&year, &month, &day, min_date, max_date);
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 5)
                    { /* 日减一；每月 1 日再减会循环到本月末日。 */
                        day = day > 1 ?          day - 1 : date_days_in_month(year, month);
                        clamp_date_range(&year, &month, &day, min_date, max_date);
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 6)
                    { /* 日加一；本月末日再加会循环到 1 日。 */
                        int dim2 = date_days_in_month(year, month);
                        day = day < dim2 ? day + 1 : 1;
                        clamp_date_range(&year, &month, &day, min_date, max_date);
                        redraw_date_dialog(title, year, month, day, dlg_left, dlg_top, dlg_right,
                                           dlg_bottom, dialog_w, side_w, gap_to_value,
                                           gap_after_value, spacing, group_w, small_group_w,
                                           group_h, y_top, group_total1, group_total2, start_x,
                                           btns);
                        continue;
                    }
                    else if (id == 7)
                    { /* 确认：通过 date 指针把三个局部值写回调用者。 */
                        date->year = year;
                        date->month = month;
                        date->day = day;
                        return 1;
                    }
                }
            }
            /* 取消按钮 id 为 0，或点击对话框外部，都返回 0 且不修改 *date。 */
            if (point_in_rect(mx, my, &btns[7].rect))
                return 0;
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom)
                return 0;
        }
        /* 没有消息时让出 CPU；这不会影响选中的日期数值。 */
        Sleep(10);
    }
}

/* 不带上下界限制的日期选择器，供车辆购入日期等场景使用。 */
int prompt_date_field(const wchar_t *title, Date *date, const Date *current)
{
    return prompt_date_field_bounded(title, date, current, NULL, NULL);
}

/* 将违章枚举转换为宽字符串常量；返回内容不可由调用者修改。 */
const wchar_t *violation_to_text(ViolationLevel level)
{
    switch (level)
    {
    case VIOLATION_VERY_MINOR:
        return L"较轻微";
    case VIOLATION_MINOR:
        return L"轻微";
    case VIOLATION_MODERATE:
        return L"中等";
    case VIOLATION_RELATIVELY_SERIOUS:
        return L"较严重";
    case VIOLATION_SERIOUS:
        return L"严重";
    default:
        return L"无";
    }
}

/* 把 VehicleType 枚举转换为界面上显示的中文名称。 */
const wchar_t *vehicle_type_to_text(VehicleType type)
{
    switch (type)
    {
    case VEHICLE_TYPE_MOTORCYCLE:
        return L"摩托车";
    case VEHICLE_TYPE_LARGE_CAR:
        return L"大型车";
    default:
        return L"小型车";
    }
}

/*
 * 显示车型选择框。VehicleType 的三个枚举值从 0 连续排列，所以按钮编号
 * 可以直接转换为枚举值；确认前不会修改调用者传入的 type。
 */
int prompt_vehicle_type_field(const wchar_t *title, VehicleType *type, VehicleType current)
{
    const wchar_t *names[] = {L"摩托车", L"小型车", L"大型车"};
    const int option_count = 3;
    const int dialog_width = 520;
    const int dialog_height = 190;
    const int left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_width) / 2;
    const int top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_height) / 2;
    const int right = left + dialog_width;
    const int bottom = top + dialog_height;
    const int button_width = 105;
    const int button_height = 42;
    const int spacing = 14;
    const int total_width = 4 * button_width + 3 * spacing;
    const int start_x = left + (dialog_width - total_width) / 2;
    const int button_y = top + 112;
    Button buttons[4];
    RECT title_rect = {left + 18, top + 12, right - 18, top + 52};
    RECT prompt_rect = {left + 18, top + 58, right - 18, top + 96};
    ExMessage message;
    int i;

    if (!type || current < VEHICLE_TYPE_MOTORCYCLE || current > VEHICLE_TYPE_LARGE_CAR)
        return 0;

    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(left, top, right, bottom);
    setbkmode(TRANSPARENT);
    settextstyle(22, 0, L"Microsoft YaHei UI");
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    settextstyle(18, 0, L"Microsoft YaHei UI");
    draw_text_rect(L"请选择车辆类型：", prompt_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    for (i = 0; i < option_count; ++i)
    {
        buttons[i].rect = {start_x + i * (button_width + spacing), button_y,
                           start_x + i * (button_width + spacing) + button_width,
                           button_y + button_height};
        buttons[i].label = names[i];
        buttons[i].id = i;
        if (i == (int)current)
        {
            setfillcolor(RGB(230, 245, 255));
            setlinecolor(RGB(100, 140, 180));
            solidrectangle(buttons[i].rect.left, buttons[i].rect.top, buttons[i].rect.right,
                           buttons[i].rect.bottom);
            settextcolor(RGB(20, 60, 100));
            draw_text_rect(buttons[i].label, buttons[i].rect,
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            draw_button(&buttons[i]);
        }
    }

    buttons[3].rect = {start_x + 3 * (button_width + spacing), button_y,
                       start_x + 3 * (button_width + spacing) + button_width,
                       button_y + button_height};
    buttons[3].label = L"取消";
    buttons[3].id = -1;
    draw_button(&buttons[3]);
    FlushBatchDraw();

    while (1)
    {
        if (peekmessage(&message, EM_MOUSE, 1) && message.message == WM_LBUTTONDOWN)
        {
            for (i = 0; i < 4; ++i)
            {
                if (!point_in_rect(message.x, message.y, &buttons[i].rect))
                    continue;
                if (buttons[i].id < 0)
                    return 0;
                *type = (VehicleType)buttons[i].id;
                return 1;
            }
            if (message.x < left || message.x > right || message.y < top || message.y > bottom)
                return 0;
        }
        Sleep(10);
    }
}

/*
 * 绘制违章等级选择框。level 是输出指针，确认后把选中的整数枚举值写回。
 *
 * names 数组顺序必须与 ViolationLevel 从 0 开始的枚举顺序保持一致。
 */
int prompt_violation_field(const wchar_t *title, ViolationLevel *level, ViolationLevel current)
{
    const wchar_t *names[] = {L"无", L"较轻微", L"轻微", L"中等", L"较严重", L"严重"};
    const int count = 6;

    /* 对话框固定大小，并在内容区域中居中。 */
    int dialog_w = 480;
    int dialog_h = 200;
    int dlg_left = CONTENT_LEFT + ((CONTENT_RIGHT - CONTENT_LEFT) - dialog_w) / 2;
    int dlg_top = CONTENT_TOP + ((CONTENT_BOTTOM - CONTENT_TOP) - dialog_h) / 2;
    int dlg_right = dlg_left + dialog_w;
    int dlg_bottom = dlg_top + dialog_h;

    /* 绘制背景、标题和提示文字。 */
    setlinecolor(RGB(120, 130, 150));
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(dlg_left, dlg_top, dlg_right, dlg_bottom);
    settextstyle(22, 0, L"Segoe UI");
    settextcolor(RGB(35, 40, 50));
    RECT title_rect = {dlg_left + 16, dlg_top + 12, dlg_right - 16, dlg_top + 48};
    draw_text_rect(title, title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    settextstyle(18, 0, L"Segoe UI");
    RECT desc_rect = {dlg_left + 16, dlg_top + 48, dlg_right - 16, dlg_top + 84};
    /* 对话框文案与车辆表头保持一致。 */
    draw_text_rect(L"请选择违章记录：", desc_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    /* 两行布局：第一行最多四个等级，余下等级与取消按钮放到第二行。 */
    int max_first = 4;
    int first_count = (count < max_first) ? count : max_first;
    int remaining = count - first_count; /* 第二行还需放置的等级数量。 */

    int btn_w = 110;
    int btn_h = 40;
    int spacing = 12;

    /* 每行按自身总宽度分别居中，第二行还要多容纳一个取消按钮。 */
    int row1_total_w = first_count * btn_w + (first_count - 1) * spacing;
    int start_x_row1 = dlg_left + (dialog_w - row1_total_w) / 2;
    int row1_y = dlg_top + 100; /* 第一行纵坐标。 */

    int second_elements = (remaining > 0 ? remaining : 0) + 1; /* 加 1 计入取消按钮。 */
    int row2_total_w = second_elements * btn_w + (second_elements - 1) * spacing;
    int start_x_row2 = dlg_left + (dialog_w - row2_total_w) / 2;
    int row2_y = dlg_bottom - btn_h - 20; /* 第二行纵坐标。 */

    Button btns[8];
    int idx = 0;

    /* 绘制第一行；idx 同时是按钮数组下标和对应枚举值。 */
    for (int i = 0; i < first_count; ++i, ++idx)
    {
        btns[idx].rect.left = start_x_row1 + i * (btn_w + spacing);
        btns[idx].rect.top = row1_y;
        btns[idx].rect.right = btns[idx].rect.left + btn_w;
        btns[idx].rect.bottom = row1_y + btn_h;
        btns[idx].label = names[idx];
        btns[idx].id = idx;
        if (idx == (int)current)
        {
            setfillcolor(RGB(230, 245, 255));
            setlinecolor(RGB(100, 140, 180));
            solidrectangle(btns[idx].rect.left, btns[idx].rect.top, btns[idx].rect.right,
                           btns[idx].rect.bottom);
            settextcolor(RGB(20, 60, 100));
            settextstyle(18, 0, L"Segoe UI");
            draw_text_rect(btns[idx].label, btns[idx].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            draw_button(&btns[idx]);
        }
    }

    /* 绘制第二行余下等级；opt 是 ViolationLevel 的实际整数值。 */
    for (int j = 0; j < remaining; ++j, ++idx)
    {
        int opt = first_count + j;
        btns[idx].rect.left = start_x_row2 + j * (btn_w + spacing);
        btns[idx].rect.top = row2_y;
        btns[idx].rect.right = btns[idx].rect.left + btn_w;
        btns[idx].rect.bottom = row2_y + btn_h;
        btns[idx].label = names[opt];
        btns[idx].id = opt;
        if (opt == (int)current)
        {
            setfillcolor(RGB(230, 245, 255));
            setlinecolor(RGB(100, 140, 180));
            solidrectangle(btns[idx].rect.left, btns[idx].rect.top, btns[idx].rect.right,
                           btns[idx].rect.bottom);
            settextcolor(RGB(20, 60, 100));
            settextstyle(18, 0, L"Segoe UI");
            draw_text_rect(btns[idx].label, btns[idx].rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else
        {
            draw_button(&btns[idx]);
        }
    }

    /* 把取消按钮追加到第二行末尾，以 -1 区分所有合法等级 0~5。 */
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
    while (1)
    {
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN)
        {
            int mx = msg.x;
            int my = msg.y;
            for (int i = 0; i < idx; ++i)
            {
                if (point_in_rect(mx, my, &btns[i].rect))
                {
                    if (btns[i].id >= 0 && btns[i].id < count)
                    {
                        *level = (ViolationLevel)btns[i].id;
                        return 1;
                    }
                    return 0; /* id=-1，用户取消。 */
                }
            }
            if (mx < dlg_left || mx > dlg_right || my < dlg_top || my > dlg_bottom)
                return 0;
        }
        Sleep(10);
    }
}

/* 点在矩形四条边界之间时返回真；这是所有按钮命中检测的基础。 */
int point_in_rect(int x, int y, const RECT *rect)
{
    return x >= rect->left && x <= rect->right && y >= rect->top && y <= rect->bottom;
}
/* 设置线条/填充颜色并绘制实心矩形。COLORREF 是 Windows 颜色类型。 */
static void fill_rect_color(int left, int top, int right, int bottom, COLORREF line, COLORREF fill)
{
    setlinecolor(line);
    setfillcolor(fill);
    solidrectangle(left, top, right, bottom);
}

void draw_text_rect(const wchar_t *text, RECT rect, UINT format)
{
    drawtext(text, &rect, format);
}

/* 根据 Button.rect 绘制背景，再将 label 居中画入同一矩形。 */
void draw_button(const Button *button)
{
    fill_rect_color(button->rect.left, button->rect.top, button->rect.right, button->rect.bottom,
                    RGB(210, 220, 235), RGB(245, 248, 252));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(35, 40, 50));
    draw_text_rect(button->label, button->rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void draw_title(const wchar_t *title)
{
    settextcolor(RGB(25, 35, 55));
    setbkmode(TRANSPARENT);
    settextstyle(34, 0, L"Segoe UI");
    outtextxy(30, 28, title);
}

void draw_status(const AppContext *ctx)
{
    wchar_t status[128] = {0};
    wchar_t user[64] = {0};
    if (app_is_logged_in(ctx))
        char_to_wchar(ctx->current_user, user, 64);
    if (app_is_logged_in(ctx))
        _snwprintf_s(status, 128, _TRUNCATE, L"当前用户：%s", user);
    else
        wcsncpy_s(status, 128, L"当前用户：未登录", _TRUNCATE);
    settextstyle(20, 0, L"Segoe UI");
    settextcolor(RGB(80, 90, 110));
    outtextxy(30, 72, status);
}

void draw_message_box(const GuiState *state)
{
    RECT rect = {CONTENT_LEFT, 20, CONTENT_RIGHT, 88};
    fill_rect_color(rect.left, rect.top, rect.right, rect.bottom, RGB(220, 228, 238),
                    RGB(251, 253, 255));
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(70, 80, 95));
    draw_text_rect(state->message, rect, DT_LEFT | DT_VCENTER | DT_WORDBREAK);
}

void draw_content_panel(void)
{
    fill_rect_color(CONTENT_LEFT, CONTENT_TOP, CONTENT_RIGHT, CONTENT_BOTTOM, RGB(220, 228, 238),
                    RGB(255, 255, 255));
}

void draw_table_header(const wchar_t *text)
{
    RECT rect = {CONTENT_LEFT + 18, CONTENT_TOP + 15, CONTENT_RIGHT - 18, CONTENT_TOP + 45};
    settextstyle(18, 0, L"Consolas");
    settextcolor(RGB(55, 65, 80));
    draw_text_rect(text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/*
 * 绘制多行表格文字。wchar_t** 可理解为“字符串地址数组的首地址”；
 * i % 2 交替选择背景色，形成斑马纹以提高可读性。
 */
void draw_rows(const wchar_t **lines, int line_count)
{
    int i;
    settextstyle(18, 0, L"Consolas");
    for (i = 0; i < line_count; ++i)
    {
        int top = CONTENT_TOP + 55 + i * ROW_HEIGHT;
        fill_rect_color(CONTENT_LEFT + 12, top, CONTENT_RIGHT - 12, top + ROW_HEIGHT - 4,
                        RGB(238, 242, 247), i % 2 == 0 ? RGB(250, 252, 255) : RGB(244, 248, 252));
        RECT rect = {CONTENT_LEFT + 22, top + 4, CONTENT_RIGHT - 20, top + ROW_HEIGHT - 4};
        settextcolor(RGB(45, 55, 72));
        draw_text_rect(lines[i], rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

/* 计算总页数并显示页码；内部页码从 0 开始，展示给用户时加 1。 */
void draw_footer_page(int page, int total_items)
{
    PageRange range = make_page_range(page, total_items);
    wchar_t page_text[64] = {0};
    _snwprintf_s(page_text, 64, _TRUNCATE, L"第 %d / %d 页，共 %d 条", range.page + 1,
                 range.total_pages, total_items);
    RECT rect = {CONTENT_LEFT + 20, CONTENT_BOTTOM - 40, CONTENT_RIGHT - 150, CONTENT_BOTTOM - 10};
    settextstyle(18, 0, L"Segoe UI");
    settextcolor(RGB(90, 100, 120));
    draw_text_rect(page_text, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/*
 * 把一次鼠标点击转换成按钮操作编号。
 *
 * buttons 指向当前页面实际显示的按钮数组，count 是有效元素数，x/y 是
 * EasyX 事件中的鼠标坐标。函数从前向后检查矩形，命中就返回 Button.id；
 * 全部未命中返回 0，事件循环随后不会执行任何实际功能。
 */
int hit_test_buttons(const Button *buttons, int count, int x, int y)
{
    int i;
    /* &buttons[i].rect 取得第 i 个按钮矩形的地址，交给通用几何判断读取。 */
    for (i = 0; i < count; ++i)
    {
        /* 找到第一个命中项就立即返回。正常布局中按钮不重叠，因此无需继续。 */
        if (point_in_rect(x, y, &buttons[i].rect))
            return buttons[i].id;
    }
    /* 0 在各 Action 枚举中保留为“没有操作”，不会和真实按钮编号冲突。 */
    return 0;
}

/* 集中修正页码并计算当前页对应的左闭右开下标范围。 */
PageRange make_page_range(int requested_page, int total_items)
{
    PageRange result;

    if (total_items < 0)
        total_items = 0;
    result.total_pages = total_items == 0 ?         1 : (total_items + PAGE_SIZE - 1) / PAGE_SIZE;
    result.page = requested_page;
    if (result.page < 0)
        result.page = 0;
    if (result.page >= result.total_pages)
        result.page = result.total_pages - 1;
    result.start = result.page * PAGE_SIZE;
    result.end = result.start + PAGE_SIZE;
    if (result.end > total_items)
        result.end = total_items;
    return result;
}
