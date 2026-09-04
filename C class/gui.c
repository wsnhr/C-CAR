#define _CRT_SECURE_NO_WARNINGS
#include "gui_internal.h"

static void draw_ui(const AppContext* ctx, const GuiState* state)
{
    setbkcolor(RGB(236, 242, 248));
    cleardevice();
    if (state->screen == SCREEN_HOME) {
        if (is_admin(ctx)) {
            Button buttons[] = {
                {{20, 120, 200, 165}, L"注册", HOME_REGISTER},
                {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
                {{20, 240, 200, 285}, L"登出", HOME_LOGOUT},
                {{20, 300, 200, 345}, L"车辆管理", HOME_CARS},
                {{20, 360, 200, 405}, L"保单管理", HOME_INSURANCE},
                {{20, 420, 200, 465}, L"删除用户", HOME_DELETE_USER},
                {{20, 480, 200, 525}, L"退出", HOME_EXIT}
            };
            draw_home(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        } else {
            Button buttons[] = {
                {{20, 120, 200, 165}, L"注册", HOME_REGISTER},
                {{20, 180, 200, 225}, L"登录", HOME_LOGIN},
                {{20, 240, 200, 285}, L"登出", HOME_LOGOUT},
                {{20, 300, 200, 345}, L"车辆管理", HOME_CARS},
                {{20, 360, 200, 405}, L"保单管理", HOME_INSURANCE},
                {{20, 420, 200, 465}, L"退出", HOME_EXIT}
            };
            draw_home(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        }
    } else if (state->screen == SCREEN_CARS) {
        if (is_admin(ctx)) {
            Button buttons[] = {
                {{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                {{20, 172, 200, 212}, L"查看全部", CAR_LIST_ALL},
                {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                {{20, 492, 200, 532}, L"返回", CAR_BACK}
            };
            draw_cars(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        } else {
            Button buttons[] = {
                {{20, 120, 200, 160}, L"添加车辆", CAR_ADD},
                {{20, 224, 200, 264}, L"查看我的", CAR_LIST_MINE},
                {{20, 276, 200, 316}, L"查找车辆", CAR_FIND},
                {{20, 328, 200, 368}, L"编辑车辆", CAR_MODIFY},
                {{20, 380, 200, 420}, L"删除车辆", CAR_DELETE},
                {{20, 432, 95, 472}, L"上一页", CAR_PREV},
                {{125, 432, 200, 472}, L"下一页", CAR_NEXT},
                {{20, 492, 200, 532}, L"返回", CAR_BACK}
            };
            draw_cars(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
        }
    } else {
        Button buttons[] = {
            {{20, 120, 200, 160}, L"添加保单", INS_ADD_POLICY},
            {{20, 172, 200, 212}, L"删除保单", INS_DELETE_POLICY},
            {{20, 224, 200, 264}, L"查询保单", INS_FIND_POLICY},
            {{20, 276, 200, 316}, L"保单列表", INS_LIST_POLICIES},
            {{20, 328, 200, 368}, L"新增理赔", INS_ADD_CLAIM},
            {{20, 380, 200, 420}, L"理赔列表", INS_LIST_CLAIMS},
            {{20, 432, 200, 472}, L"结案", INS_SETTLE_CLAIM},
            {{20, 484, 95, 524}, L"上一页", INS_PREV},
            {{125, 484, 200, 524}, L"下一页", INS_NEXT},
            {{20, 544, 200, 584}, L"返回", INS_BACK}
        };
        draw_insurance(ctx, state, buttons, (int)(sizeof(buttons) / sizeof(buttons[0])));
    }
    FlushBatchDraw();
}

void gui_run(AppContext* ctx)
{
    load_users(ctx);
    load_cars(ctx);
    load_policies(ctx);
    // 在加载保单后立即更新保单状态（自动过期）
    update_policy_active_status(ctx);
    load_claims(ctx);

    GuiState state; memset(&state, 0, sizeof(state)); state.screen = SCREEN_HOME; state.insurance_view = VIEW_POLICY_LIST; set_message(&state, L"请从左侧选择功能");

    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, EX_SHOWCONSOLE);
    BeginBatchDraw();
    int running = 1;
    while (running) {
        draw_ui(ctx, &state);
        ExMessage msg;
        if (peekmessage(&msg, EM_MOUSE, 1) && msg.message == WM_LBUTTONDOWN) {
            int action = 0;
            if (state.screen == SCREEN_HOME) action = hit_test_home(ctx, msg.x, msg.y);
            else if (state.screen == SCREEN_CARS) action = hit_test_cars(ctx, msg.x, msg.y);
            else action = hit_test_insurance(msg.x, msg.y);
            if (state.screen == SCREEN_HOME) handle_home_action(ctx, &state, action, &running);
            else if (state.screen == SCREEN_CARS) handle_car_action(ctx, &state, action);
            else handle_insurance_action(ctx, &state, action);
        }
    }
    EndBatchDraw(); closegraph();
}
