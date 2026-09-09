#ifndef APP_UTILS_H
#define APP_UTILS_H

#include "app_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* 日期公共接口：业务层和 GUI 使用同一套日期规则。 */
    Date date_today(void);
    int date_compare(const Date *left, const Date *right);
    int date_days_in_month(int year, int month);
    int date_is_valid(const Date *date);

    /*
     * 将 source 完整复制到 dest。成功返回 1；参数无效或目标容量不足返回 0。
     * 本函数不会悄悄截断业务字段，失败时把 dest 设为空字符串。
     */
    int copy_text(char *dest, size_t capacity, const char *source);

    /* 业务输入验证。max_length 表示目标字符数组的总容量。 */
    int validate_plate(const char *plate);
    int validate_string_len(const char *text, int max_length);
    int validate_price_value(double value);

#ifdef __cplusplus
}
#endif

#endif /* APP_UTILS_H */
