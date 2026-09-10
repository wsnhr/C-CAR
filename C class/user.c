#define _CRT_SECURE_NO_WARNINGS
#include "user.h"
#include "app_utils.h"
#include "car.h"
#include "insurance.h"
#include "password_hash.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * user.c —— 用户注册、登录和登录状态模块
 *
 * 本模块只负责账号数据和身份规则，不读取控制台，也不直接写文件。
 * current_user 为空表示未登录，固定用户名 admin 表示管理员。
 */

/* 顺序检查用户名。只遍历 users[0] 到 users[user_count-1]。 */
int user_exists(const AppContext *ctx, const char *username)
{
    int i;

    if (!ctx || !username)
        return 0;
    for (i = 0; i < ctx->user_count; i++)
    {
        if (strcmp(ctx->users[i].username, username) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/*
 * 返回 users 数组中匹配记录的地址，找不到返回 NULL。返回的是数组内部地址，
 * 调用者可以修改该 User，但不能 free；static 表示此辅助函数不公开给其他文件。
 */
static User *find_user(AppContext *ctx, const char *username)
{
    int i;
    if (!ctx || !username)
        return NULL;
    for (i = 0; i < ctx->user_count; ++i)
        if (strcmp(ctx->users[i].username, username) == 0)
            return &ctx->users[i];
    return NULL;
}

/*
 * 验证姓名的本地格式。unsigned char 避免把 UTF-8/本地编码中的高位字节当成
 * 负数传给 isalpha；ASCII 字符只允许英文字母，中文等高位字节交由原编码保留。
 * 这只是格式检查，不等于连接公安或其他权威系统完成实名认证。
 */
int validate_real_name(const char *real_name)
{
    const unsigned char *current;
    if (!validate_string_len(real_name, REAL_NAME_CAPACITY) || strlen(real_name) < 2)
        return 0;
    for (current = (const unsigned char *)real_name; *current; ++current)
        if (*current < 0x80 && !isalpha(*current))
            return 0;
    return 1;
}

/*
 * 校验 18 位中国居民身份证号码：前 17 位必须为数字；每位乘固定权重求和，
 * 再用余数确定第 18 位校验码。随后从第 7~14 位取出出生年月日，复用公共日期
 * 函数排除不存在或晚于今天的日期。toupper 让末位 x 和 X 都能通过比较。
 */
int validate_chinese_id_card(const char *id_card)
{
    static const int weights[17] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
    static const char checks[] = "10X98765432";
    Date birth_date;
    int i, sum = 0;

    if (!id_card || strlen(id_card) != 18)
        return 0;
    for (i = 0; i < 17; ++i)
    {
        if (!isdigit((unsigned char)id_card[i]))
            return 0;
        sum += (id_card[i] - '0') * weights[i];
    }
    if (toupper((unsigned char)id_card[17]) != checks[sum % 11])
        return 0;
    if (strncmp(id_card, "000000", 6) == 0 || strncmp(id_card + 14, "000", 3) == 0)
        return 0;

    birth_date.year = (id_card[6] - '0') * 1000 + (id_card[7] - '0') * 100 +
                      (id_card[8] - '0') * 10 + (id_card[9] - '0');
    birth_date.month = (id_card[10] - '0') * 10 + (id_card[11] - '0');
    birth_date.day = (id_card[12] - '0') * 10 + (id_card[13] - '0');
    {
        Date today = date_today();
        return date_is_valid(&birth_date) && date_compare(&birth_date, &today) <= 0;
    }
}

/*
 * 把姓名与规范化后的身份证号拼成“姓名|身份证号”，作为实名凭据的哈希输入。
 * output_capacity 由调用者传入，snprintf 返回本来需要写入的字符数；只有返回值
 * 小于容量时才表示没有截断。组合文本只存在于当前函数调用期间，不写入文件。
 */
static int make_identity_text(const char *real_name, const char *id_card, char *output,
                              size_t output_capacity)
{
    char normalized_id[ID_CARD_CAPACITY];
    int written;
    if (!output || !validate_real_name(real_name) || !validate_chinese_id_card(id_card))
        return 0;
    if (!copy_text(normalized_id, sizeof(normalized_id), id_card))
        return 0;
    normalized_id[17] = (char)toupper((unsigned char)normalized_id[17]);
    written = snprintf(output, output_capacity, "%s|%s", real_name, normalized_id);
    return written > 0 && (size_t)written < output_capacity;
}

/* ================= 统一认证与登录状态接口 ================= */
/*
 * 判断是否登录。&& 会短路：ctx 为 NULL 时不会继续访问 ctx->current_user，
 * 因而不会发生空指针访问。
 */
int app_is_logged_in(const AppContext *ctx)
{
    return ctx && ctx->current_user[0] != '\0';
}
/* 只有已登录且 current_user 与 "admin" 完全相等时才返回真。 */
int app_is_admin(const AppContext *ctx)
{
    return app_is_logged_in(ctx) && strcmp(ctx->current_user, "admin") == 0;
}
/*
 * 统一注册接口。依次校验指针、空串、容量、保留名和重复名，然后把数据
 * 写入 users[user_count]。密码先加随机盐并计算 SHA-256，结构体中只保存
 * 盐和哈希的十六进制文本。文件保存由 GUI 控制层统一负责。
 */
int register_user_account(AppContext *ctx, const char *username, const char *password,
                          const char *real_name, const char *id_card)
{
    User *user;
    char identity_text[REAL_NAME_CAPACITY + ID_CARD_CAPACITY + 1];

    if (!ctx || !validate_string_len(username, (int)sizeof(ctx->users[0].username)) ||
        !validate_string_len(password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;

    if (ctx->user_count >= MAX_USERS)
        return 0;

    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    user = &ctx->users[ctx->user_count];
    memset(user, 0, sizeof(*user));
    if (!copy_text(user->username, sizeof(user->username), username) ||
        !password_hash_create(password, user->salt, user->password) ||
        !password_hash_create(identity_text, user->identity_salt, user->identity_hash))
    {
        memset(user, 0, sizeof(*user));
        return 0;
    }
    ctx->user_count++;
    return 1;
}

/*
 * 找回密码的核心业务接口：先用姓名和身份证组合验证 identity_hash，再生成新的
 * 密码盐和哈希。新值先写入局部数组，全部计算成功后才替换 User 中的旧值，
 * 避免生成过程中失败而把账号留在“只更新了一半”的状态。管理员账号是内置
 * 特殊账号，没有实名哈希，因此明确禁止通过此流程重置。
 */
int reset_user_password(AppContext *ctx, const char *username, const char *real_name,
                        const char *id_card, const char *new_password)
{
    User *user;
    char identity_text[REAL_NAME_CAPACITY + ID_CARD_CAPACITY + 1];
    char new_salt[PASSWORD_SALT_HEX_CAPACITY];
    char new_hash[PASSWORD_HASH_HEX_CAPACITY];

    if (!ctx || !username || strcmp(username, "admin") == 0 ||
        !validate_string_len(new_password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;
    user = find_user(ctx, username);
    if (!user || !user->identity_salt[0] || !user->identity_hash[0] ||
        !password_hash_matches(identity_text, user->identity_salt, user->identity_hash))
        return 0;
    if (!password_hash_create(new_password, new_salt, new_hash))
        return 0;
    copy_text(user->salt, sizeof(user->salt), new_salt);
    copy_text(user->password, sizeof(user->password), new_hash);
    return 1;
}
/*
 * 统一登录接口。管理员是内置特殊分支；普通用户先按用户名找到记录，
 * 再用同一盐值重新计算输入密码的哈希。哈希匹配后才设置 current_user。
 */
int login_user_account(AppContext *ctx, const char *username, const char *password)
{
    int i;

    if (!ctx || !username || !password)
        return 0;

    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0)
    {
        copy_text(ctx->current_user, sizeof(ctx->current_user), "admin");
        return 1;
    }

    for (i = 0; i < ctx->user_count; i++)
    {
        if (strcmp(ctx->users[i].username, username) == 0 &&
            password_hash_matches(password, ctx->users[i].salt, ctx->users[i].password))
        {
            copy_text(ctx->current_user, sizeof(ctx->current_user), username);
            return 1;
        }
    }

    return 0;
}

/*
 * 查找某个用户名对应的失败记录；没有记录时在数组尾部新建一项。
 * 不存在的用户名统一映射为 "<unknown>"，既避免泄露账号是否存在，也防止
 * 攻击者不断更换不存在的名字来无限创建记录。
 */
static LoginAttempt *find_or_create_login_attempt(AppContext *ctx, const char *username)
{
    const char *key;
    int i;

    if (!ctx || !username)
        return NULL;
    key = strcmp(username, "admin") == 0 || user_exists(ctx, username) ? username : "<unknown>";

    for (i = 0; i < ctx->login_attempt_count; ++i)
    {
        if (strcmp(ctx->login_attempts[i].username, key) == 0)
            return &ctx->login_attempts[i];
    }
    if (ctx->login_attempt_count >= MAX_LOGIN_ATTEMPTS)
        return NULL;

    i = ctx->login_attempt_count++;
    memset(&ctx->login_attempts[i], 0, sizeof(ctx->login_attempts[i]));
    copy_text(ctx->login_attempts[i].username, sizeof(ctx->login_attempts[i].username), key);
    return &ctx->login_attempts[i];
}

/*
 * 登录保护流程只比较时间点，不调用 Sleep，因此不会冻结图形界面。
 * 第五次失败时记录“当前时间 + 60 秒”；用户以后再次点击登录时再判断是否
 * 已经过了解锁时间。成功登录会清除该用户名的全部失败记录。
 */
LoginResult login_user_account_guarded(AppContext *ctx, const char *username, const char *password,
                                       int *remaining_attempts, int *remaining_seconds)
{
    LoginAttempt *attempt;
    long long now;

    if (remaining_attempts)
        *remaining_attempts = 0;
    if (remaining_seconds)
        *remaining_seconds = 0;
    if (!ctx || !username || !password)
        return LOGIN_RESULT_WRONG_CREDENTIALS;

    attempt = find_or_create_login_attempt(ctx, username);
    if (!attempt)
        return LOGIN_RESULT_LOCKED;
    now = (long long)time(NULL);

    if (attempt->locked_until > now)
    {
        if (remaining_seconds)
            *remaining_seconds = (int)(attempt->locked_until - now);
        return LOGIN_RESULT_LOCKED;
    }
    if (attempt->locked_until != 0)
    {
        attempt->locked_until = 0;
        attempt->failed_count = 0;
    }

    if (login_user_account(ctx, username, password))
    {
        attempt->failed_count = 0;
        attempt->locked_until = 0;
        return LOGIN_RESULT_SUCCESS;
    }

    ++attempt->failed_count;
    if (attempt->failed_count >= LOGIN_MAX_FAILURES)
    {
        attempt->locked_until = now + LOGIN_LOCK_SECONDS;
        if (remaining_seconds)
            *remaining_seconds = LOGIN_LOCK_SECONDS;
        return LOGIN_RESULT_LOCKED;
    }
    if (remaining_attempts)
        *remaining_attempts = LOGIN_MAX_FAILURES - attempt->failed_count;
    return LOGIN_RESULT_WRONG_CREDENTIALS;
}

/* 把第一个字符设为 '\0'，即可让 current_user 成为空字符串。 */
void logout_current_user(AppContext *ctx)
{
    if (ctx)
        ctx->current_user[0] = '\0';
}

/*
 * 删除用户的业务事务：先删除其车辆及车辆保险，再清理剩余保险数据，最后
 * 从用户数组移除账号。函数不保存文件、不写日志，调用者统一处理这些副作用。
 */
int remove_user_cascade(AppContext *ctx, const char *username)
{
    int i;

    if (!ctx || !app_is_admin(ctx) || !username || strcmp(username, "admin") == 0 ||
        !user_exists(ctx, username))
        return 0;

    for (i = 0; i < ctx->car_count;)
    {
        if (strcmp(ctx->cars[i].owner, username) == 0)
            remove_car(ctx, ctx->cars[i].plate);
        else
            ++i;
    }
    remove_insurance_for_user(ctx, username);

    for (i = 0; i < ctx->user_count; ++i)
    {
        int j;
        if (strcmp(ctx->users[i].username, username) != 0)
            continue;
        for (j = i; j < ctx->user_count - 1; ++j)
            ctx->users[j] = ctx->users[j + 1];
        --ctx->user_count;
        memset(&ctx->users[ctx->user_count], 0, sizeof(ctx->users[0]));
        if (strcmp(ctx->current_user, username) == 0)
            logout_current_user(ctx);
        return 1;
    }
    return 0;
}
