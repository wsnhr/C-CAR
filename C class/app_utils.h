#ifndef APP_UTILS_H
#define APP_UTILS_H

/*
 * app_utils.h —— 公共日期、字符串和输入校验接口
 *
 * 头文件只声明“怎样调用”，具体算法在 app_utils.c。业务模块包含本文件后，
 * 可以共享同一套规则，而不需要复制实现。const 指针表示函数只读取参数。
 */

#include "app_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* 返回本机今天日期；系统时间转换失败时返回 {0, 0, 0}。 */
    Date date_today(void);

    /* left 早于/等于/晚于 right 时分别返回 -1/0/1。 */
    int date_compare(const Date *left, const Date *right);

    /* 返回某年某月天数；月份非法返回 0，2 月会自动处理闰年。 */
    int date_days_in_month(int year, int month);

    /* 检查年份、月份和日期组合是否合法，合法返回 1。 */
    int date_is_valid(const Date *date);

    /*
     * 将 source 完整复制到 dest。成功返回 1；参数无效或目标容量不足返回 0。
     * 本函数不会悄悄截断业务字段，失败时把 dest 设为空字符串。
     */
    int copy_text(char *dest, size_t capacity, const char *source);

    /* plate 只能包含英文字母、数字和 '-'，并且必须能放入 Car.plate。 *///？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？？
    int validate_plate(const char *plate);

    /* max_length 是目标 char 数组总容量，已经包含末尾 '\0' 的位置。 */
    int validate_string_len(const char *text, int max_length);

    /* 当前课程项目允许的金额范围是 0~1e9。 */
    int validate_price_value(double value);

#ifdef __cplusplus
}
#endif

#endif /* APP_UTILS_H */
