/*
 * gui_internal.h —— GUI 模块内部共享的类型、常量和函数声明
 *
 * gui.c、gui_common.c 和三个 gui_* 页面文件都包含本文件。业务模块不应
 * 包含它，因为其中的 EasyX/Windows 类型只是界面实现细节。
 */
#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include "app_types.h"
#include "app_utils.h"
#include "car.h"
#include "gui.h"
#include "insurance.h"
#include "storage.h"
#include "user.h"
#include <ctype.h>
#include <graphics.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>

/* GUI 布局宏：所有页面共用同一坐标系，窗口左上角是 (0, 0)。 */
#define WINDOW_WIDTH 1000  /* 窗口宽度，单位为像素。 */
#define WINDOW_HEIGHT 700  /* 窗口高度。 */
#define CONTENT_LEFT 240   /* 右侧主内容区左边界。 */
#define CONTENT_RIGHT 970  /* 主内容区右边界。 */
#define CONTENT_TOP 110    /* 主内容区上边界。 */
#define CONTENT_BOTTOM 660 /* 主内容区下边界。 */
#define ROW_HEIGHT 34      /* 表格每一行的高度。 */
#define PAGE_SIZE 12       /* 每页最多显示 12 条记录。 */

/* 一个按钮由矩形范围、显示文字和操作编号构成。 */
typedef struct Button
{
    RECT rect;            /* Windows 矩形：left/top/right/bottom。 */
    const wchar_t *label; /* 宽字符串；中文界面使用 L"文字"。 */
    int id;               /* 点击后交给事件处理函数的操作编号。 */
} Button;

/*
 * enum 为一组相关整数取有类型含义的名字。相比多个 #define，调试器能直接
 * 显示枚举成员，编译器也更容易提示类型问题；未指定值会从 0 依次加 1。
 */
typedef enum
{
    SCREEN_HOME,
    SCREEN_CARS,
    SCREEN_INSURANCE
} Screen;
/* 三类操作使用 1、101、201 三个编号段，避免不同页面操作混淆。 */
typedef enum
{
    HOME_REGISTER = 1,
    HOME_LOGIN,
    HOME_FORGOT_PASSWORD,
    HOME_LOGOUT,
    HOME_CARS,
    HOME_INSURANCE,
    HOME_DELETE_USER,
    HOME_EXIT
} HomeAction;
typedef enum
{
    CAR_ADD = 101,
    CAR_FIND,
    CAR_SORT,
    CAR_MODIFY,
    CAR_DELETE,
    CAR_PREV,
    CAR_NEXT,
    CAR_BACK
} CarAction;
typedef enum
{
    INS_ADD_POLICY = 201,
    INS_DELETE_POLICY,
    INS_FIND_POLICY,
    INS_LIST_POLICIES,
    INS_ADD_CLAIM,
    INS_LIST_CLAIMS,
    INS_CANCEL_CLAIM,
    INS_REVIEW_CLAIM,
    INS_SETTLE_CLAIM,
    INS_PREV,
    INS_NEXT,
    INS_BACK
} InsuranceAction;
typedef enum
{
    VIEW_POLICY_LIST,
    VIEW_CLAIM_LIST
} InsuranceView;

/* AppContext 保存业务数据；GuiState 只保存界面的临时显示状态。 */
typedef struct GuiState
{
    Screen screen;                /* 当前一级页面。 */
    int car_page;                 /* 车辆页码，从 0 开始。 */
    int policy_page;              /* 保单列表当前页码，从 0 开始，不是总页数。 */
    int claim_page;               /* 理赔页码。 */
    int car_show_mine;            /* 0=全部可见，1=只看本人。 */
    int car_search_active;         /* 0=普通列表，1=按 car_search 筛选列表。 */
    CarSearchCondition car_search; /* 车辆多条件查询的当前筛选值。 */
    CarSortField car_sort_field;   /* 当前排序字段；0 表示保持登记顺序。 */
    int car_sort_ascending;        /* 1=升序，0=降序；默认排序时忽略。 */
    InsuranceView insurance_view; /* 当前显示保单还是理赔。 */
    wchar_t message[512];         /* 底部提示消息缓冲区。 */
} GuiState;

/*
 * 分页计算结果。page 是边界修正后的当前页；total_pages 是总页数；start 是
 * 本页第一条在“筛选结果下标数组”中的位置；end 是后一位置，不属于本页。
 * 例如 start=12、end=15 表示遍历下标 12、13、14，即左闭右开 [12,15)。
 */
typedef struct PageRange
{
    int page;
    int total_pages;
    int start;
    int end;
} PageRange;

/*
 * 以下公共工具实现在 gui_common.c。dest_count/count 表示调用者提供的数组
 * 容量，工具函数依靠它限制写入范围，避免 char/wchar_t 缓冲区越界。
 */
void set_message(GuiState *state, const wchar_t *text);
void char_to_wchar(const char *src, wchar_t *dest, int dest_count);
void wchar_to_char(const wchar_t *src, char *dest, int dest_count);
int prompt_char_field(const wchar_t *title, const wchar_t *prompt, char *dest, int dest_count,
                      const char *default_value);
/* 与上面函数不同：用户确认空字符串也返回 1，供“留空表示不限”的查询框使用。 */
int prompt_optional_char_field(const wchar_t *title, const wchar_t *prompt, char *dest,
                               int dest_count, const char *default_value);
int prompt_double_field(const wchar_t *title, const wchar_t *prompt, double *value,
                        double default_value);
int prompt_date_field(const wchar_t *title, Date *date, const Date *current);
/* 带上下界限制的日期选择器；min_date/max_date 为 NULL 表示不限制该侧。 */
int prompt_date_field_bounded(const wchar_t *title, Date *date, const Date *current,
                              const Date *min_date, const Date *max_date);
int prompt_vehicle_type_field(const wchar_t *title, VehicleType *type, VehicleType current);
int prompt_violation_field(const wchar_t *title, ViolationLevel *level, ViolationLevel current);
/*
 * 显示统一的 EasyX 确认框：确认返回 1，取消或点击框外返回 0。
 * dangerous 非 0 时使用红色警告样式，否则使用蓝色普通确认样式。
 */
int show_confirm_dialog(const wchar_t *title, const wchar_t *message,
                        const wchar_t *confirm_label, int dangerous);
const wchar_t *violation_to_text(ViolationLevel level);
const wchar_t *vehicle_type_to_text(VehicleType type);
int point_in_rect(int x, int y, const RECT *rect);
void draw_text_rect(const wchar_t *text, RECT rect, UINT format);
void draw_button(const Button *button);
void draw_title(const wchar_t *title);
void draw_status(const AppContext *ctx);
void draw_message_box(const GuiState *state);
void draw_content_panel(void);
void draw_table_header(const wchar_t *text);
void draw_rows(const wchar_t **lines, int line_count);
void draw_footer_page(int page, int total_items);
int hit_test_buttons(const Button *buttons, int count, int x, int y);
PageRange make_page_range(int requested_page, int total_items);

/*
 * 页面分为三步：draw_* 绘制；hit_test_* 把鼠标坐标变为操作编号；
 * handle_*_action 根据编号调用业务函数并更新界面状态。
 */
void draw_home(const AppContext *ctx, const GuiState *state);
void handle_home_action(AppContext *ctx, GuiState *state, int action, int *running);
int hit_test_home(const AppContext *ctx, int x, int y);

void draw_cars(const AppContext *ctx, const GuiState *state);
void handle_car_action(AppContext *ctx, GuiState *state, int action);
int hit_test_cars(const AppContext *ctx, int x, int y);

void draw_insurance(const AppContext *ctx, const GuiState *state);
void handle_insurance_action(AppContext *ctx, GuiState *state, int action);
int hit_test_insurance(const AppContext *ctx, int x, int y);

#endif
