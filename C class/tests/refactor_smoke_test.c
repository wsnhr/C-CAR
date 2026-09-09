#include "../app_types.h"
#define _CRT_SECURE_NO_WARNINGS

#include "../app_utils.h"
#include "../car.h"
#include "../insurance.h"
#include "../password_hash.h"
#include "../storage.h"
#include "../user.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * 这组测试不启动 GUI，也不读写业务数据文件，只验证重构后最关键的数据关系、
 * 权限和级联删除。assert 条件为假时程序会立即终止并报告失败位置。
 */
int main(void)
{
    AppContext ctx;
    Date purchase_date = {2024, 1, 15};
    PolicyTerms terms = {"车损险", {2020, 1, 1}, {2030, 12, 31}, 0.8, 1.2};
    Car *car;
    AppContext loaded;
    FILE *legacy_file;

    init_app(&ctx);
    assert(ctx.user_count == 0 && ctx.car_count == 0);
    assert(register_user_account(&ctx, "alice", "alice-pass"));
    assert(register_user_account(&ctx, "bob", "bob-pass"));
    assert(!register_user_account(&ctx, "alice", "duplicate"));
    /* 内存中保存的是 32 位盐和 64 位哈希，不能与明文相同。 */
    assert(strlen(ctx.users[0].salt) == 32);
    assert(strlen(ctx.users[0].password) == 64);
    assert(strcmp(ctx.users[0].password, "alice-pass") != 0);
    assert(!login_user_account(&ctx, "alice", "wrong-pass"));
    /* 固定向量验证算法确实是 SHA256(salt || password)，与上游文件格式兼容。 */
    assert(
        password_hash_matches("test-pass", "000102030405060708090a0b0c0d0e0f",
                              "eb177309970710aff1b7263d3f1bdddb763f1463e7fabdb8d032eda5978d670b"));

    assert(login_user_account(&ctx, "alice", "alice-pass"));
    /* 普通用户即使请求 bob，也只能把车辆登记到自己名下。 */
    assert(add_car_for_current_user(&ctx, "A-100", "Brand", "Model", "bob", VIOLATION_NONE,
                                    purchase_date, 100000.0));
    car = find_car(&ctx, "A-100");
    assert(car && strcmp(car->owner, "alice") == 0);

    assert(add_policy_for_current_user(&ctx, "P-100", "A-100", "alice", &terms));
    assert(add_claim_for_current_user(&ctx, "C-100", "P-100", "alice", "repair", 1000.0));
    assert(ctx.policy_count == 1 && ctx.claim_count == 1);
    assert(ctx.claims[0].status == CLAIM_PENDING);
    /* 普通用户无权审核或结案；管理员必须先审核通过再结案。 */
    assert(!review_claim_for_current_user(&ctx, "C-100", 1));
    assert(!settle_claim_for_current_user(&ctx, "C-100"));
    assert(login_user_account(&ctx, "admin", "123456"));
    assert(review_claim_for_current_user(&ctx, "C-100", 1));
    assert(ctx.claims[0].status == CLAIM_APPROVED);
    assert(!review_claim_for_current_user(&ctx, "C-100", 0));
    assert(settle_claim_for_current_user(&ctx, "C-100"));
    assert(ctx.claims[0].status == CLAIM_SETTLED);

    /* 测试在临时工作目录运行，因此这些保存不会接触正式业务数据文件。 */
    assert(save_users(&ctx) && save_cars(&ctx) && save_policies(&ctx) && save_claims(&ctx));
    init_app(&loaded);
    assert(load_users(&loaded) && load_cars(&loaded) && load_policies(&loaded) &&
           load_claims(&loaded));
    assert(loaded.user_count == 2 && loaded.car_count == 1 && loaded.policy_count == 1 &&
           loaded.claim_count == 1);
    assert(loaded.claims[0].status == CLAIM_SETTLED);
    assert(login_user_account(&loaded, "alice", "alice-pass"));

    /* 删除车辆必须同时删除引用它的保单和理赔。 */
    assert(remove_car_for_current_user(&ctx, "A-100"));
    assert(ctx.car_count == 0 && ctx.policy_count == 0 && ctx.claim_count == 0);

    assert(add_car_for_current_user(&ctx, "A-200", "Brand", "Model", "alice", VIOLATION_MINOR,
                                    purchase_date, 80000.0));
    assert(remove_user_cascade(&ctx, "alice"));
    assert(!user_exists(&ctx, "alice"));
    assert(ctx.car_count == 0 && ctx.policy_count == 0 && ctx.claim_count == 0);

    /* save_users 必须全量重写；再次加载后不能重新出现已删除的 alice。 */
    assert(save_users(&ctx));
    init_app(&loaded);
    assert(load_users(&loaded));
    assert(loaded.user_count == 1 && strcmp(loaded.users[0].username, "bob") == 0);

    /* 旧版 cars.txt 只有四个字符串；逐行解析后缺失字段应保持默认值。 */
    legacy_file = fopen("cars.txt", "w");
    assert(legacy_file);
    fputs("OLD-1 Brand Model bob\n", legacy_file);
    fclose(legacy_file);
    init_app(&loaded);
    assert(load_cars(&loaded));
    assert(loaded.car_count == 1 && strcmp(loaded.cars[0].plate, "OLD-1") == 0);
    assert(loaded.cars[0].purchase_price == 0.0 && loaded.cars[0].purchase_date.year == 0);

    assert(date_days_in_month(2024, 2) == 29);
    assert(date_is_valid(&purchase_date));
    assert(!validate_plate("INVALID@"));
    puts("Business and persistence smoke tests passed.");
    return 0;
}
