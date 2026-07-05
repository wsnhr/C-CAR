#ifndef GUI_H
#define GUI_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* 图形界面模块：负责启动 EasyX 图形界面 */
	void gui_run(AppContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */