#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include <graphics.h>
#include <windows.h>
#include <wchar.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "common.h"
#include "gui.h"

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 700
#define CONTENT_LEFT 240
#define CONTENT_RIGHT 970
#define CONTENT_TOP 110
#define CONTENT_BOTTOM 660
#define ROW_HEIGHT 34
#define PAGE_SIZE 12

typedef struct Button {
    RECT rect;
    const wchar_t* label;
    int id;
} Button;

typedef enum { SCREEN_HOME, SCREEN_CARS, SCREEN_INSURANCE } Screen;
typedef enum { HOME_REGISTER = 1, HOME_LOGIN, HOME_LOGOUT, HOME_CARS, HOME_INSURANCE, HOME_DELETE_USER, HOME_EXIT } HomeAction;
typedef enum { CAR_ADD = 101, CAR_LIST_ALL, CAR_LIST_MINE, CAR_FIND, CAR_MODIFY, CAR_DELETE, CAR_PREV, CAR_NEXT, CAR_BACK } CarAction;
typedef enum { INS_ADD_POLICY = 201, INS_DELETE_POLICY, INS_FIND_POLICY, INS_LIST_POLICIES, INS_ADD_CLAIM, INS_LIST_CLAIMS, INS_SETTLE_CLAIM, INS_PREV, INS_NEXT, INS_BACK } InsuranceAction;
typedef enum { VIEW_POLICY_LIST, VIEW_CLAIM_LIST } InsuranceView;

typedef struct GuiState {
    Screen screen;
    int car_page;
    int policy_page;
    int claim_page;
    int car_show_mine;
    InsuranceView insurance_view;
    wchar_t message[512];
} GuiState;

void set_message(GuiState* state, const wchar_t* text);
void char_to_wchar(const char* src, wchar_t* dest, int dest_count);
void wchar_to_char(const wchar_t* src, char* dest, int dest_count);
int prompt_char_field(const wchar_t* title, const wchar_t* prompt, char* dest, int dest_count, const char* default_value);
int prompt_double_field(const wchar_t* title, const wchar_t* prompt, double* value, double default_value);
int prompt_date_field(const wchar_t* title, Date* date, const Date* current);
int prompt_violation_field(const wchar_t* title, ViolationLevel* level, ViolationLevel current);
const wchar_t* violation_to_text(ViolationLevel level);
int is_logged_in(const AppContext* ctx);
int is_admin(const AppContext* ctx);
int point_in_rect(int x, int y, const RECT* rect);
void draw_text_rect(const wchar_t* text, RECT rect, UINT format);
void draw_button(const Button* button);
void draw_title(const wchar_t* title);
void draw_status(const AppContext* ctx);
void draw_message_box(const GuiState* state);
void draw_content_panel(void);
void draw_table_header(const wchar_t* text);
void draw_rows(const wchar_t** lines, int line_count);
void draw_footer_page(int page, int total_items);
int hit_test_buttons(const Button* buttons, int count, int x, int y);
int compare_date_local(const Date* a, const Date* b);
int validate_plate(const char* plate);
int validate_string_len(const char* s, int maxlen);
int validate_price_value(double value);
int validate_date(const Date* date);

void draw_home(const AppContext* ctx, const GuiState* state, const Button* buttons, int count);
void handle_home_action(AppContext* ctx, GuiState* state, int action, int* running);
int hit_test_home(const AppContext* ctx, int x, int y);

void draw_cars(const AppContext* ctx, const GuiState* state, const Button* buttons, int count);
void handle_car_action(AppContext* ctx, GuiState* state, int action);
int hit_test_cars(const AppContext* ctx, int x, int y);

void draw_insurance(const AppContext* ctx, const GuiState* state, const Button* buttons, int count);
void handle_insurance_action(AppContext* ctx, GuiState* state, int action);
int hit_test_insurance(int x, int y);

#endif
