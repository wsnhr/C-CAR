#ifndef GUI_H
#define GUI_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    gui.h
    图形界面模块头文件（基于 EasyX）

    说明：
    - 本模块提供一个简单的图形化入口函数 `start_gui`，用于在控制台程序中弹出 EasyX 窗口，
      展示车辆、保单、当前用户等信息并允许鼠标交互。
    - 使用前请确保已在工程中正确配置 EasyX（包含目录与链接库），并在 Windows 环境下编译运行。
    - 函数实现位于 `gui.c`，仅做展示与演示用途。如需更复杂的交互（表单输入、文本框等），
      建议引入更完善的 GUI 库或扩展 EasyX 功能模块。

    注意：该头文件只声明对外接口，内部实现细节（鼠标事件处理、绘图）在实现文件中维护。
*/

// 启动图形界面（基于 EasyX 实现）
// 调用后进入图形界面，关闭图形窗口后返回到控制台程序
void gui_run(AppContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // GUI_H