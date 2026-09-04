#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
#include "gui.h"

int main(void)
{
    AppContext ctx;
    init_app(&ctx);
    gui_run(&ctx);
    return 0;
}
