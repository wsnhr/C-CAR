#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
#include "gui.h"

int main(void)
{
//	FreeConsole(); // 关闭控制台窗口
    AppContext ctx;
    init_app(&ctx);
    gui_run(&ctx);
    return 0;
}
