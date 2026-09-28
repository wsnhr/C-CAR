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
 * user.c —— 用户注册、登录、实名校验和登录状态模块
 *
 * GUI 层只收集输入和显示提示，真正的账号规则集中在本文件。这里会修改
 * AppContext 中的用户数组、current_user 和登录失败记录，但不会弹输入框，
 * 也不会直接保存文件；保存和日志由调用它的 GUI 控制层统一处理。
 */

/*
 * 判断用户名是否已经注册。只读取 users[0..user_count-1]，找到返回 1，
 * 遍历结束仍未找到返回 0。const AppContext* 保证查询不会误改用户数据。
 */
int user_exists(const AppContext *ctx, const char *username)
{
    /* i 是用户数组下标；先检查指针，避免 strcmp 收到无效地址。 */
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
    /* 返回 &ctx->users[i]，后续对 *user 的修改就是对原数组元素的修改。 */
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
    /* 使用 unsigned char* 逐字节扫描，避免中文编码的高位字节以负数形式传给
       isalpha；该函数只接受 unsigned char 范围或 EOF。 */
    const unsigned char *current;
    /* 先使用统一长度规则，并额外要求姓名至少占两个字节。 */
    if (!validate_string_len(real_name, REAL_NAME_CAPACITY) || strlen(real_name) < 2)
        return 0;
    /* *current 为 '\0' 时结束。ASCII 范围内只允许字母；中文等高位字节在
       这里保留。这只是本地格式筛选，不是真实身份认证。 */
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
    /* weights 与 checks 来自身份证校验码规则。static const 只初始化一次，
       只允许读取，并且数组名称不会暴露到本文件之外。 */
    static const int weights[17] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
    static const char checks[] = "10X98765432";
    Date birth_date;
    int i, sum = 0;

    if (!id_card || strlen(id_card) != 18)
        return 0;
    /* 前 17 位逐位验证并累加。“数字字符 - '0'”得到实际整数，再乘本位权重。 */
    for (i = 0; i < 17; ++i)
    {
        if (!isdigit((unsigned char)id_card[i]))
            return 0;
        sum += (id_card[i] - '0') * weights[i];
    }
    /* sum % 11 产生 0~10 下标，据此从 checks 取出正确的末位校验字符。 */
    if (toupper((unsigned char)id_card[17]) != checks[sum % 11])
        return 0;
    //地址码不能全部为0,顺序码不能全部为0
    if (strncmp(id_card, "000000", 6) == 0 || strncmp(id_card + 14, "000", 3) == 0)
        return 0;

    /* 第 7~14 位是 YYYYMMDD。用十进制位权把字符组合成 Date 的整数成员。 */
    birth_date.year = (id_card[6] - '0') * 1000 + (id_card[7] - '0') * 100 +
                      (id_card[8] - '0') * 10 + (id_card[9] - '0');
    birth_date.month = (id_card[10] - '0') * 10 + (id_card[11] - '0');
    birth_date.day = (id_card[12] - '0') * 10 + (id_card[13] - '0');
    {
        /* 日期必须真实存在且不能晚于今天；&& 短路保证无效日期不参与比较。 */
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
    //空指针
    if (!output || !validate_real_name(real_name) || !validate_chinese_id_card(id_card))
        return 0;
    /* 先复制到局部数组再统一末位大小写，避免修改调用者传入的身份证字符串。 */
    if (!copy_text(normalized_id, sizeof(normalized_id), id_card))
        return 0;
    normalized_id[17] = (char)toupper((unsigned char)normalized_id[17]);
    /* '|' 明确分隔两个字段。snprintf 返回原本需要的长度，可据此判断是否截断。"%s|%s", */
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
    /* user 稍后指向数组尾部空槽；identity_text 是实名哈希的临时明文输入，
       函数结束即失效，不会直接保存在 User 结构体中。 */
    User *user;
    char identity_text[REAL_NAME_CAPACITY + ID_CARD_CAPACITY + 1];

    /* 先完成全部不修改 ctx 的校验。|| 会短路，前项失败后不会继续执行后项。指针、空串、容量、 */
    if (!ctx || !validate_string_len(username, (int)sizeof(ctx->users[0].username)) ||
        !validate_string_len(password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;

    /* users 是固定数组，写入前必须确认还有空槽。 */
    if (ctx->user_count >= MAX_USERS)
        return 0;
    //保留名和重复名
    if (strcmp(username, "admin") == 0 || user_exists(ctx, username))
        return 0;

    /* user_count 也是下一个空槽下标。暂不增加计数，直到整条记录完全成功。 */
    user = &ctx->users[ctx->user_count];
    memset(user, 0, sizeof(*user));
    /* 三步都成功才算注册完成：复制用户名、生成密码盐/哈希、生成实名盐/哈希。password_hash_create写入盐和密文
       password_hash_create 通过数组参数把结果直接写进 User 对应字段。 */
    if (!copy_text(user->username, sizeof(user->username), username) ||
        !password_hash_create(password, user->salt, user->password) ||
        !password_hash_create(identity_text, user->identity_salt, user->identity_hash))
    {
        /* 任一步失败都清空待写槽，避免留下只有部分字段的半成品记录。 */
        memset(user, 0, sizeof(*user));
        return 0;
    }
    /* 最后增加计数，相当于提交记录；其他遍历从此可以看到新用户。 */
    ctx->user_count++;
    return 1;
}

/*
 * 忘记密码的核心业务接口：先用姓名和身份证组合验证 identity_hash，再生成新的
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
    //常规检验
    if (!ctx || !username || strcmp(username, "admin") == 0 ||
        !validate_string_len(new_password, PASSWORD_INPUT_CAPACITY) ||
        !make_identity_text(real_name, id_card, identity_text, sizeof(identity_text)))
        return 0;
    /* 返回 users 数组内部地址。验证通过前暂不覆盖旧密码，失败时账号仍可用。 */
    user = find_user(ctx, username);

	//这里比较的是实名组合的哈希
    if (!user || !user->identity_salt[0] || !user->identity_hash[0] ||
        !password_hash_matches(identity_text, user->identity_salt, user->identity_hash))
        return 0;
    /* 新盐和新哈希先写入局部数组，全部生成成功后再一起替换旧字段。 */
    if (!password_hash_create(new_password, new_salt, new_hash))
        return 0;
    /* 这里才真正修改 User；函数本身不保存 users.txt，由 GUI 调用者负责。 */
    copy_text(user->salt, sizeof(user->salt), new_salt);
    copy_text(user->password, sizeof(user->password), new_hash);
    return 1;
}



/*
 * 验证用户名和密码，并在成功时建立登录状态。
 *
 * ctx      ：应用数据。函数读取 ctx->users；成功后把登录名复制到
 *            ctx->current_user。
 * username ：GUI 收集到的用户名，只读，不在本函数中修改。
 * password ：GUI 收集到的明文密码，只在本次验证期间使用，不保存到结构体。
 * 返回值   ：1 表示验证成功并已更新 current_user，0 表示参数无效或凭据错误。
 *
 * 管理员是内置特殊分支；普通用户则遍历 users[0..user_count-1]。找到同名
 * 用户后，password_hash_matches 使用该用户原有的盐重新计算输入密码的哈希，
 * 再与已保存哈希比较。哈希匹配后才更新 current_user。
 */
int login_user_account(AppContext *ctx, const char *username, const char *password)
{
    /* i 是普通用户数组的循环下标，只访问 user_count 以内的有效元素。 */
    int i;

    /* 先拒绝空指针，避免后面的 strcmp 解引用无效地址。 */
    if (!ctx || !username || !password)
        return 0;

    /* 管理员不占用 users 数组中的一项。验证成功后把固定用户名复制到
       current_user，后续 app_is_admin 通过该字符串识别管理员身份。 */
    if (strcmp(username, "admin") == 0 && strcmp(password, "123456") == 0)
    {
        copy_text(ctx->current_user, sizeof(ctx->current_user), "admin");
        return 1;
    }

    /* strcmp 返回 0 才表示两个用户名内容完全相同。&& 具有短路特性：只有
       用户名匹配时才计算密码哈希，避免为无关用户做不必要的密码运算。 */
    /* strcmp 返回 0 表示两个字符串逐字符相等。找到后立即返回，无需继续扫描。 */
    for (i = 0; i < ctx->user_count; i++)
    {
        if (strcmp(ctx->users[i].username, username) == 0 &&
            password_hash_matches(password, ctx->users[i].salt, ctx->users[i].password))
        {
            /* copy_text 进行带容量限制的复制。current_user 保存登录名而不是
               User 指针，因此用户数组移动后不会留下悬空指针。 */
            copy_text(ctx->current_user, sizeof(ctx->current_user), username);
            return 1;
        }
    }

    /* 遍历完仍没有同时匹配用户名和密码，登录失败且不改变 current_user。 */
    return 0;
}

/*
 * 查找或创建某个用户名对应的登录失败记录。
 *
 * 返回 LoginAttempt 指针，调用者可通过该指针直接更新 failed_count 和
 * locked_until；数组已满或参数无效时返回 NULL。不存在的用户名统一映射为
 * "<unknown>"，既避免提示差异泄露账号是否存在，也防止攻击者不断更换虚假
 * 用户名耗尽固定大小的 login_attempts 数组。
 */
static LoginAttempt *find_or_create_login_attempt(AppContext *ctx, const char *username)
{
    const char *key;
    int i;

    if (!ctx || !username)
        return NULL;
    /* ?: 是条件运算符。真实账号使用自己的键，不存在的名字统一映射为
       <unknown>，既节省数组容量，也不泄露账号是否存在。 */
    key = strcmp(username, "admin") == 0 || user_exists(ctx, username) ? username : "<unknown>";

    /* 优先返回已有记录的地址，让调用者直接修改其中的计数和锁定时间。 */
    for (i = 0; i < ctx->login_attempt_count; ++i)
    {
        if (strcmp(ctx->login_attempts[i].username, key) == 0)
            return &ctx->login_attempts[i];
    }

    //满记录
    if (ctx->login_attempt_count >= MAX_LOGIN_ATTEMPTS)
        return NULL;

    /* 后缀 ++ 先把旧计数赋给 i 作为新槽下标，再把有效记录数增加 1。 */
    i = ctx->login_attempt_count++;
    memset(&ctx->login_attempts[i], 0, sizeof(ctx->login_attempts[i]));
    copy_text(ctx->login_attempts[i].username, sizeof(ctx->login_attempts[i].username), key);//箭头指针，点对象
    return &ctx->login_attempts[i];
}

/*
 * 在普通凭据验证外增加“连续失败次数 + 临时锁定”保护。
 *
 * ctx / username / password 与 login_user_account 的含义相同。
 * remaining_attempts ：可选输出参数。非 NULL 时写入本轮失败后的剩余次数。
 * remaining_seconds  ：可选输出参数。非 NULL 时写入距离解锁还剩多少秒。
 * 返回值             ：SUCCESS、WRONG_CREDENTIALS 或 LOCKED 三种枚举之一。
 *
 * 这两个输出参数允许传 NULL，表示调用者不关心对应信息。因此每次写入前都要
 * 先执行 if (remaining_...)。函数只比较时间点，不调用 Sleep，不会让 GUI
 * 停住 60 秒。第五次失败时保存“当前时间 + 60”；下次登录时重新比较时间。
 */
LoginResult login_user_account_guarded(AppContext *ctx, const char *username, const char *password,
                                       int *remaining_attempts, int *remaining_seconds)
{
    LoginAttempt *attempt;
    long long now;

    /* 先把可选输出初始化为 0。这样即使后面提前返回，调用者也不会读到未
       初始化的随机值。指针为 NULL 时跳过写入，避免解引用空指针。 */
    if (remaining_attempts)
        *remaining_attempts = 0;
    if (remaining_seconds)
        *remaining_seconds = 0;
    if (!ctx || !username || !password)
        return LOGIN_RESULT_WRONG_CREDENTIALS;

    /* 每个有效账号有独立失败记录；未知账号共同使用 <unknown> 记录。 */
    attempt = find_or_create_login_attempt(ctx, username);//这行很关键
    if (!attempt)
        return LOGIN_RESULT_LOCKED;
    /* time(NULL) 得到当前时间点（以秒表示）。转换成 long long 后可以直接和
       LoginAttempt.locked_until 比较或相减，计算剩余锁定秒数。 */
    now = (long long)time(NULL);

    /* locked_until 位于未来说明锁定尚未结束。此分支不会验证密码，即使这次
       输入正确也必须等到锁定时间结束。返回还有多久 */
    if (attempt->locked_until > now)
    {
        if (remaining_seconds)
            *remaining_seconds = (int)(attempt->locked_until - now);
        return LOGIN_RESULT_LOCKED;
    }
    /* 能执行到这里说明旧锁定时间已经过去。把时间和失败次数一起清零，让
       用户重新获得完整的尝试次数。 */
    if (attempt->locked_until != 0)
    {
        attempt->locked_until = 0;
        attempt->failed_count = 0;
    }

    /* 保护层不重复实现密码检查，而是调用统一登录接口。成功时该接口已经
       写入 current_user，本层只负责清除历史失败状态。 */
    if (login_user_account(ctx, username, password))
    {
        attempt->failed_count = 0;
        attempt->locked_until = 0;
        return LOGIN_RESULT_SUCCESS;
    }

    /* 验证失败后先累计次数。前置 ++ 表示立即把字段中的值增加 1。 */
    ++attempt->failed_count;
    if (attempt->failed_count >= LOGIN_MAX_FAILURES)
    {
        /* 保存未来的解锁时间点，而非在这里等待。事件循环仍可继续绘图和处理
           其他按钮，之后再次登录时由上面的时间比较决定是否解锁。 */
        attempt->locked_until = now + LOGIN_LOCK_SECONDS;
        if (remaining_seconds)
            *remaining_seconds = LOGIN_LOCK_SECONDS;
        return LOGIN_RESULT_LOCKED;
    }
    /* 尚未达到锁定阈值时，把剩余机会写回 GUI 层用于提示。 */
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

    /* remove_car 会把后续车辆左移，所以删除成功后不增加 i：原下一项已经
       移到相同位置，需要继续检查。只有当前车不属于目标用户时才 ++i。 */
    for (i = 0; i < ctx->car_count;)
    {
        if (strcmp(ctx->cars[i].owner, username) == 0)
            remove_car(ctx, ctx->cars[i].plate);
        else
            ++i;
    }
    /* 再清理由该用户直接持有、但没有随车辆删除而消失的保险记录。 */
    remove_insurance_for_user(ctx, username);

    for (i = 0; i < ctx->user_count; ++i)
    {
        int j;
        if (strcmp(ctx->users[i].username, username) != 0)
            continue;
        /* 固定数组不能保留中间空洞，把目标后的用户逐个向左覆盖一格。 */
        for (j = i; j < ctx->user_count - 1; ++j)
            ctx->users[j] = ctx->users[j + 1];
        --ctx->user_count;
        /* count 减一后指向新的尾部无效槽，清零以移除最后一份重复内容。 */
        memset(&ctx->users[ctx->user_count], 0, sizeof(ctx->users[0]));
        if (strcmp(ctx->current_user, username) == 0)
            logout_current_user(ctx);
        return 1;
    }
    return 0;
}
