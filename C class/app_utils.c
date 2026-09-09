#define _CRT_SECURE_NO_WARNINGS
#include "app_utils.h"

#include <string.h>
#include <time.h>

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

int date_is_valid(const Date *date)
{
    int max_day;

    if (!date || date->year < 1900 || date->year > 3000)
        return 0;
    max_day = date_days_in_month(date->year, date->month);
    return max_day > 0 && date->day >= 1 && date->day <= max_day;
}

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

static int plate_char_is_valid(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') || value == '-';
}

int validate_plate(const char *plate)
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

int validate_string_len(const char *text, int max_length)
{
    size_t length;

    if (!text || max_length <= 0)
        return 0;
    length = strlen(text);
    return length > 0 && length < (size_t)max_length;
}

int validate_price_value(double value)
{
    return value >= 0.0 && value <= 1e9;
}
