#ifndef APP_TYPES_H
#define APP_TYPES_H

/*
 * app_types.h —— 只保存数据类型和容量常量
 *
 * 本文件不声明业务函数，因此用户、车辆、保险、存储和 GUI 模块都可以安全
 * 包含它，不会因为某个模块的实现细节形成循环依赖。
 */

/* 宏在编译前替换为数字，同时决定固定数组容量和业务上限。 */
#define MAX_USERS 100
#define MAX_CARS 100
#define MAX_POLICIES 100
#define MAX_CLAIMS 200

/* 容量包含字符串末尾的 '\0'，所以可输入字符数比容量少 1。 */
#define USERNAME_CAPACITY 20
#define PASSWORD_INPUT_CAPACITY 20
#define PASSWORD_SALT_HEX_CAPACITY 33
#define PASSWORD_HASH_HEX_CAPACITY 65

/* typedef 为匿名结构体取名 User，以后可直接写 User user;。 */
typedef struct
{
    char username[USERNAME_CAPACITY];          /* 用户名，最多 19 个有效字符。 */
    char salt[PASSWORD_SALT_HEX_CAPACITY];     /* 16 字节随机盐的 32 位十六进制文本。 */
    char password[PASSWORD_HASH_HEX_CAPACITY]; /* SHA-256 结果的 64 位十六进制文本，不是明文。 */
} User;

/* 枚举让整数等级拥有可读名称，未指定值的成员从前一项依次加 1。 */
typedef enum
{
    VIOLATION_NONE = 0,           /* 无违章 */
    VIOLATION_VERY_MINOR,         /* 极轻微 */
    VIOLATION_MINOR,              /* 轻微 */
    VIOLATION_MODERATE,           /* 一般 */
    VIOLATION_RELATIVELY_SERIOUS, /* 较严重 */
    VIOLATION_SERIOUS             /* 严重 */
} ViolationLevel;

/* 日期按值保存；合法性由 app_utils.h 中的 date_is_valid 检查。 */
typedef struct
{
    int year;  /* 1900~3000 */
    int month; /* 1~12 */
    int day;   /* 1~当月最大天数 */
} Date;

/* 车辆通过 owner 用户名关联 User。 */
typedef struct
{
    char plate[10];           /* 唯一车牌，最多 9 个字符。 */
    char brand[20];           /* 品牌。 */
    char model[20];           /* 型号。 */
    char owner[20];           /* 对应 User.username。 */
    ViolationLevel violation; /* 影响保险计算。 */
    Date purchase_date;       /* 购入日期。 */
    double purchase_price;    /* 购入价格。 */
} Car;

/* 保单通过 plate 关联 Car，owner 用于权限过滤。 */
typedef struct
{
    char policy_id[24];     /* 唯一保单号。 */
    char plate[10];         /* 对应 Car.plate。 */
    char owner[20];         /* 投保人用户名。 */
    int active;             /* 0=未生效/已失效，1=当前有效。 */
    double coverage_limit;  /* 最高保障金额。 */
    double premium;         /* 保费。 */
    Date start_date;        /* 生效日期。 */
    Date end_date;          /* 失效日期。 */
    char coverage_desc[64]; /* 产品说明。 */
} Policy;

/* 理赔状态按照“提交 -> 审核 -> 结案”的业务流程变化。 */
typedef enum
{
    CLAIM_PENDING = 0,  /* 待审核：用户已提交，等待管理员处理。 */
    CLAIM_APPROVED = 1, /* 已通过：管理员审核通过，可以结案。 */
    CLAIM_SETTLED = 2,  /* 已结案：审核通过后由管理员完成结案。 */
    CLAIM_REJECTED = 3  /* 已驳回：管理员审核不通过。 */
} ClaimStatus;

/* 理赔通过 policy_id 关联 Policy，并冗余保存车牌以便显示和清理。 */
typedef struct
{
    char claim_id[24];      /* 唯一理赔号。 */
    char policy_id[24];     /* 对应 Policy.policy_id。 */
    char plate[10];         /* 涉及车辆。 */
    char claimant[20];      /* 申请人用户名。 */
    char description[256];  /* 理赔说明。 */
    double request_amount;  /* 申请金额。 */
    double approved_amount; /* 规则计算后的批准金额。 */
    int status;             /* 值来自 ClaimStatus；使用 int 便于文本文件读写。 */
    Date claim_date;        /* 申请日期。 */
} Claim;

/* 所有数组都使用“前 count 项有效”的固定容量设计。 */
typedef struct
{
    User users[MAX_USERS];         /* 用户数组。 */
    int user_count;                /* users 前多少项有效。 */
    Car cars[MAX_CARS];            /* 车辆数组。 */
    int car_count;                 /* cars 前多少项有效。 */
    Policy policies[MAX_POLICIES]; /* 保单数组。 */
    int policy_count;              /* policies 前多少项有效。 */
    Claim claims[MAX_CLAIMS];      /* 理赔数组。 */
    int claim_count;               /* claims 前多少项有效。 */
    char current_user[20];         /* 空串=未登录，否则为当前用户名。 */
} AppContext;

#endif /* APP_TYPES_H */
