#ifndef GUI_H
#define GUI_H

/*
 * gui.h 是 GUI 模块对外公开的最小接口。main.c 只需要知道如何启动界面，
 * 不需要看到按钮、坐标和绘图细节；这些内部定义放在 gui_internal.h。
 * 上下两组条件编译指令是头文件保护以及 C/C++ 兼容处理。
 */

#include "app_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * 创建 EasyX 窗口、加载数据并进入事件循环。
     * ctx 指向 main.c 创建的总数据容器，因此 GUI 与业务层操作同一份数据。
     * void 表示窗口关闭后不向调用者返回额外计算结果。
     */
    void gui_run(AppContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
