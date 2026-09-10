#define _CRT_SECURE_NO_WARNINGS
#include "app_utils.h"

#include <string.h>
#include <time.h>

/*
 * app_utils.c —— 与具体业务对象无关的公共工具实现
 *
 * 车辆、保险、用户和 GUI 都需要日期、字符串和输入校验。如果每个模块各写
 * 一份，规则很容易逐渐不一致，所以这些函数集中在本文件中。这里不访问
 * AppContext，也不读写文件；只需传入数据，并根据返回值判断成功失败。
 */

/*
 * 读取本机当前日期。Date result = {0} 会把三个成员全部初始化为 0；如果
 * 系统时间转换失败，就返回这个“零日期”，后续 date_is_valid 会判定它无效。
 * struct tm 的年份从 1900 开始计数，月份从 0 开始，要分别加 1900 和 1。
 */
Date date_today(void)
{
    Date result = {0};
    time_t now = time(NULL);
    struct tm local_time;

#ifdef _WIN32
    if (localtime_s(&local_time, &now) != 0)
        return result;
#else
    if (!localtime_r(&now, &local_time))
        return result;
#endif

    result.year = local_time.tm_year + 1900;
    result.month = local_time.tm_mon + 1;
    result.day = local_time.tm_mday;
    return result;
}

/*
 * 按年、月、日依次比较两个日期：left 较早返回 -1，相同返回 0，较晚返回 1。
 * -> 用于通过结构体指针访问成员；?: 是条件运算符，可理解为简写的 if/else。
 * 空指针无法比较，这里返回 0，业务调用前仍应先保证参数有效。
 */
int date_compare(const Date *left, const Date *right)
{
    if (!left || !right)
        return 0;
    if (left->year != right->year)
        return left->year < right->year ? -1 : 1;
    if (left->month != right->month)
        return left->month < right->month ? -1 : 1;
    if (left->day != right->day)
        return left->day < right->day ? -1 : 1;
    return 0;
}

/*
 * 返回指定月份的天数，月份非法时返回 0。days 声明为 static const：
 * const 表示内容不可修改；static 表示数组只创建一次，并且只在本函数可见。
 * 闰年规则是“能被 4 整除，并且不能被 100 整除；或者能被 400 整除”。
 */
int date_days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap_year;

    if (month < 1 || month > 12)
        return 0;

    leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (month == 2)
        return days[month - 1] + leap_year;
    return days[month - 1];
}

/* 先限制年份和月份，再用当月最大天数检查 day，避免 2 月 30 日等非法日期。 */
int date_is_valid(const Date *date)
{
    int max_day;

    if (!date || date->year < 1900 || date->year > 3000)
        return 0;
    max_day = date_days_in_month(date->year, date->month);
    return max_day > 0 && date->day >= 1 && date->day <= max_day;
}

/*
 * 完整复制 C 字符串。capacity 是目标数组总容量，必须包含结尾的 '\0'。
 * strlen 不计算 '\0'，所以 length >= capacity 时放不下，函数拒绝截断。
 * memcpy 的 length + 1 把字符串内容和末尾 '\0' 一起复制过去。
 */
int copy_text(char *dest, size_t capacity, const char *source)
{
    size_t length;

    if (!dest || capacity == 0)
        return 0;
    dest[0] = '\0';
    if (!source)
        return 0;

    length = strlen(source);
    if (length >= capacity)
        return 0;
    memcpy(dest, source, length + 1);
    return 1;
}

/* static 使这个辅助函数只对 app_utils.c 可见，外部模块不能直接调用。 */
static int plate_char_is_valid(char value)
{
    return (unsigned char)value >= 0x80 || /* 汉字等 GBK 多字节字符的字节 */
           (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') || value == '-';
}

/*
 * 查找键较为宽松，以便旧版 cars.txt 中的车辆仍然可以被查找和删除。
 * 它只保证字符串能放入数组，而且不包含空格或其他特殊字符。
 */
int validate_plate_key(const char *plate)
{
    const char *current;

    if (!validate_string_len(plate, (int)sizeof(((Car *)0)->plate)))
        return 0;
    for (current = plate; *current; ++current)
    {
        if (!plate_char_is_valid(*current))
            return 0;
    }
    return 1;
}

/* 把 ASCII 小写字母转换为大写；数字和短横线保持不变。 */
static char plate_char_to_upper(char value)
{
    if (value >= 'a' && value <= 'z')
        return (char)(value - 'a' + 'A');
    return value;
}

/*
 * 标准化并验证新车牌。课程项目采用“地区汉字 + 字母 + 编号”的真实车牌格式：
 * 1. 第一个字符必须是地区汉字（GBK 双字节，占 2 个字节）；
 * 2. 汉字后是 1 位字母，字母后是 3~6 位字母或数字；
 * 3. I 和 O 容易与数字 1、0 混淆，因此不允许使用；
 * 4. 输入的小写字母统一转换为大写，避免大小写不同造成重复车牌。
 */
int normalize_plate(const char *plate, char *dest, size_t dest_capacity)
{
    size_t length;
    size_t i;
    size_t body; /* 地区汉字之后、字母开始的字节下标 */

    if (!plate || !dest || dest_capacity == 0)
        return 0;
    dest[0] = '\0';
    length = strlen(plate);
    if (length >= dest_capacity || length >= sizeof(((Car *)0)->plate))
        return 0;

    /* 首字符必须是地区汉字：GBK 首字节落在 0x81~0xFE，占 2 个字节 */
    if (length < 3 || (unsigned char)plate[0] < 0x81)
        return 0;
    body = 2;

    /* 汉字后必须紧跟 1 位字母 */
    if (!((plate[body] >= 'A' && plate[body] <= 'Z') ||
          (plate[body] >= 'a' && plate[body] <= 'z')))
        return 0;

    /* 字母后是 3~6 位字母或数字 */
    if (length - body - 1 < 3 || length - body - 1 > 6)
        return 0;

    for (i = 0; i < length; ++i)
    {
        char normalized = plate[i];
        if (i >= body)
        {
            normalized = plate_char_to_upper(plate[i]);
            if (!((normalized >= 'A' && normalized <= 'Z') ||
                  (normalized >= '0' && normalized <= '9')))
                return 0;
            if (normalized == 'I' || normalized == 'O')
                return 0;
        }
        dest[i] = normalized;
    }
    dest[length] = '\0';
    return 1;
}

/* 只关心是否合法时，用局部数组接收标准化结果即可。 */
int validate_plate(const char *plate)
{
    char normalized[sizeof(((Car *)0)->plate)] = {0};
    return normalize_plate(plate, normalized, sizeof(normalized));
}

/*
 * 验证字符串能完整放进容量为 max_length 的 char 数组。循环条件不需要手写，
 * strlen 得到有效字符数；必须满足 0 < length < max_length 才能为 '\0' 留位置。
 */
int validate_string_len(const char *text, int max_length)
{
    size_t length;

    if (!text || max_length <= 0)
        return 0;
    length = strlen(text);
    return length > 0 && length < (size_t)max_length;
}

/* 把金额限制在 0 到 10 亿元之间，拒绝负数和明显异常的大数。 */
int validate_price_value(double value)
{
    return value >= 0.0 && value <= 1e9;
}
