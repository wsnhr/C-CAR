#ifndef COMMON_H
#define COMMON_H

/*
 * common.h —— 兼容性聚合头文件
 *
 * 项目接口已经按职责拆到下面六个头文件。旧代码若继续包含 common.h 仍能
 * 使用全部接口；新代码应优先只包含自己真正依赖的头文件，以便看清模块关系。
 */

#include "app_types.h"
#include "app_utils.h"
#include "car.h"
#include "insurance.h"
#include "storage.h"
#include "user.h"

#endif /* COMMON_H */
