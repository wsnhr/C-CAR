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
    Date later_date = {2025, 6, 1};
    PolicyTerms terms = {"车损险", {2020, 1, 1}, {2030, 12, 31}, 0.8, 1.2};
    Car *car;
    AppContext loaded;
    AppContext search_ctx;
    AppContext auth_ctx;
    CarSearchCondition condition;
    Car motorcycle;
    Car small_car;
    Car large_car;
    Policy payout_policy;
    char normalized_plate[10] = {0};
    int matching_indices[MAX_CARS] = {0};
    int matching_count;
    FILE *legacy_file;

    init_app(&ctx);
    assert(ctx.user_count == 0 && ctx.car_count == 0);
    assert(validate_chinese_id_card("11010519491231002X"));
    assert(validate_chinese_id_card("11010519491231002x"));
    assert(!validate_chinese_id_card("110105194912310021"));
    assert(!validate_chinese_id_card("11010519491331002X"));
    assert(register_user_account(&ctx, "alice", "alice-pass", "Alice", "11010519491231002X"));
    assert(register_user_account(&ctx, "bob", "bob-pass", "Bob", "11010519491231002X"));
    assert(!register_user_account(&ctx, "alice", "duplicate", "Alice", "11010519491231002X"));
    assert(register_user_account_detailed(&ctx, "alice", "duplicate", "Alice",
                                          "11010519491231002X") ==
           REGISTER_RESULT_DUPLICATE_USERNAME);
    assert(register_user_account_detailed(&ctx, "admin", "admin-pass", "Admin",
                                          "11010519491231002X") ==
           REGISTER_RESULT_RESERVED_USERNAME);
    assert(register_user_account_detailed(&ctx, "new-user", "new-pass", "Invalid Name",
                                          "11010519491231002X") ==
           REGISTER_RESULT_INVALID_REAL_NAME);
    /* 内存中保存的是 32 位盐和 64 位哈希，不能与明文相同。 */
    assert(strlen(ctx.users[0].salt) == 32);
    assert(strlen(ctx.users[0].password) == 64);
    assert(strcmp(ctx.users[0].password, "alice-pass") != 0);
    assert(!login_user_account(&ctx, "alice", "wrong-pass"));
    assert(!reset_user_password(&ctx, "alice", "Wrong", "11010519491231002X", "new-pass"));
    assert(reset_user_password(&ctx, "alice", "Alice", "11010519491231002x", "new-pass"));
    assert(!login_user_account(&ctx, "alice", "alice-pass"));
    assert(login_user_account(&ctx, "alice", "new-pass"));
    /* 固定向量验证算法确实是 SHA256(salt || password)，与上游文件格式兼容。 */
    assert(
        password_hash_matches("test-pass", "000102030405060708090a0b0c0d0e0f",
                              "eb177309970710aff1b7263d3f1bdddb763f1463e7fabdb8d032eda5978d670b"));

    /* 连续五次错误会锁定账号；把时间改为 0 模拟锁定已经到期。 */
    init_app(&auth_ctx);
    assert(register_user_account(&auth_ctx, "locked-user", "right-pass", "LockUser",
                                 "11010519491231002X"));
    for (int attempt_number = 1; attempt_number < LOGIN_MAX_FAILURES; ++attempt_number)
    {
        int remaining = 0;
        assert(login_user_account_guarded(&auth_ctx, "locked-user", "wrong-pass", &remaining,
                                          NULL) == LOGIN_RESULT_WRONG_CREDENTIALS);
        assert(remaining == LOGIN_MAX_FAILURES - attempt_number);
    }
    {
        int seconds = 0;
        assert(login_user_account_guarded(&auth_ctx, "locked-user", "wrong-pass", NULL,
                                          &seconds) == LOGIN_RESULT_LOCKED);
        assert(seconds == LOGIN_LOCK_SECONDS);
        assert(login_user_account_guarded(&auth_ctx, "locked-user", "right-pass", NULL,
                                          &seconds) == LOGIN_RESULT_LOCKED);
    }
    /* 时间点 1 早已过去，下一次调用会走“锁定到期并清零”的分支。 */
    auth_ctx.login_attempts[0].locked_until = 1;
    assert(login_user_account_guarded(&auth_ctx, "locked-user", "right-pass", NULL, NULL) ==
           LOGIN_RESULT_SUCCESS);

    /*
     * 多条件查询测试：各关键字必须分别出现在车牌、品牌、型号和车主字段中，
     * 违章还必须等于轻微。这样一次测试覆盖了全部五个查询条件。
     * 结果数组保存的是 cars 原数组下标，因此可以再用该下标访问完整车辆。
     */
    init_app(&search_ctx);
    assert(register_user_account(&search_ctx, "alice", "alice-pass", "Alice",
                                 "11010519491231002X"));
    assert(register_user_account(&search_ctx, "bob", "bob-pass", "Bob",
                                 "11010519491231002X"));
    assert(login_user_account(&search_ctx, "alice", "alice-pass"));
    assert(add_car_for_current_user(&search_ctx, "A-100", "BYD", "Han", "alice",
                                    VEHICLE_TYPE_SMALL_CAR, VIOLATION_MINOR, purchase_date,
                                    100000.0));
    assert(login_user_account(&search_ctx, "bob", "bob-pass"));
    assert(add_car_for_current_user(&search_ctx, "B-200", "BYD", "Song", "bob",
                                    VEHICLE_TYPE_LARGE_CAR, VIOLATION_NONE, later_date, 90000.0));
    memset(&condition, 0, sizeof(condition));
    strcpy(condition.plate_keyword, "A-");
    strcpy(condition.brand_keyword, "BY");
    strcpy(condition.model_keyword, "Ha");
    strcpy(condition.owner_keyword, "lic");
    condition.vehicle_type = VEHICLE_TYPE_SMALL_CAR;
    condition.violation = VIOLATION_MINOR;
    /* 普通用户 bob 无法看到 alice 的 A-100，所以结果为 0。 */
    matching_count = collect_matching_car_indices(&search_ctx, 0, &condition, matching_indices,
                                                  MAX_CARS);
    assert(matching_count == 0);
    assert(login_user_account(&search_ctx, "admin", "123456"));
    matching_count = collect_matching_car_indices(&search_ctx, 0, &condition, matching_indices,
                                                  MAX_CARS);
    assert(matching_count == 1);
    assert(strcmp(search_ctx.cars[matching_indices[0]].plate, "A-100") == 0);
    /* 清空其他字段并取消违章限制后，只按品牌 "BY" 查询，两辆车都应匹配。 */
    condition.plate_keyword[0] = '\0';
    condition.model_keyword[0] = '\0';
    condition.owner_keyword[0] = '\0';
    condition.vehicle_type = CAR_VEHICLE_TYPE_ANY;
    condition.violation = CAR_VIOLATION_ANY;
    matching_count = collect_matching_car_indices(&search_ctx, 0, &condition, matching_indices,
                                                  MAX_CARS);
    assert(matching_count == 2);
    /* 排序只改变下标数组：价格升序和日期降序都是 B 在前，违章降序是 A 在前。 */
    sort_car_indices(&search_ctx, matching_indices, matching_count, CAR_SORT_PRICE, 1);
    assert(strcmp(search_ctx.cars[matching_indices[0]].plate, "B-200") == 0);
    sort_car_indices(&search_ctx, matching_indices, matching_count, CAR_SORT_VIOLATION, 0);
    assert(strcmp(search_ctx.cars[matching_indices[0]].plate, "A-100") == 0);
    sort_car_indices(&search_ctx, matching_indices, matching_count, CAR_SORT_PURCHASE_DATE, 0);
    assert(strcmp(search_ctx.cars[matching_indices[0]].plate, "B-200") == 0);

    assert(login_user_account(&ctx, "alice", "new-pass"));
    /* 普通用户即使请求 bob，也只能把车辆登记到自己名下。 */
    assert(add_car_for_current_user(&ctx, "a-100", "Brand", "Model", "bob",
                                    VEHICLE_TYPE_SMALL_CAR, VIOLATION_NONE, purchase_date,
                                    100000.0));
    car = find_car(&ctx, "A-100");
    assert(car && strcmp(car->owner, "alice") == 0);
    assert(strcmp(car->plate, "A-100") == 0 && car->vehicle_type == VEHICLE_TYPE_SMALL_CAR);
    /* 大小写统一后，同一车牌不能再次添加。 */
    assert(!add_car_for_current_user(&ctx, "A-100", "Other", "Other", "alice",
                                     VEHICLE_TYPE_MOTORCYCLE, VIOLATION_NONE, purchase_date,
                                     5000.0));
    assert(!add_car_for_current_user(&ctx, "C-300", "Other", "Other", "alice",
                                     (VehicleType)99, VIOLATION_NONE, purchase_date, 5000.0));
    assert(modify_car_for_current_user(&ctx, "A-100", "Brand", "Model",
                                       VEHICLE_TYPE_LARGE_CAR, VIOLATION_NONE, purchase_date,
                                       100000.0));
    assert(car->vehicle_type == VEHICLE_TYPE_LARGE_CAR);
    assert(modify_car_for_current_user(&ctx, "A-100", "Brand", "Model",
                                       VEHICLE_TYPE_SMALL_CAR, VIOLATION_NONE, purchase_date,
                                       100000.0));

    /* 相同价格、车龄和违章下，大型车保费最高，摩托车最低。 */
    motorcycle = *car;
    small_car = *car;
    large_car = *car;
    motorcycle.vehicle_type = VEHICLE_TYPE_MOTORCYCLE;
    small_car.vehicle_type = VEHICLE_TYPE_SMALL_CAR;
    large_car.vehicle_type = VEHICLE_TYPE_LARGE_CAR;
    assert(calculate_premium_for_car(&large_car) > calculate_premium_for_car(&small_car));
    assert(calculate_premium_for_car(&small_car) > calculate_premium_for_car(&motorcycle));
    memset(&payout_policy, 0, sizeof(payout_policy));
    payout_policy.coverage_limit = 100000.0;
    /* 大型车免赔比例最高，因此其他条件相同时批准金额最低。 */
    assert(calculate_payout_for_claim(NULL, &small_car, 1000.0) == 0.0);
    assert(calculate_payout_for_claim(&payout_policy, &small_car, 1000.0) >
           calculate_payout_for_claim(&payout_policy, &motorcycle, 1000.0));
    assert(calculate_payout_for_claim(&payout_policy, &motorcycle, 1000.0) >
           calculate_payout_for_claim(&payout_policy, &large_car, 1000.0));

    assert(add_policy_for_current_user(&ctx, "P-100", "A-100", "alice", &terms));
    assert(add_claim_for_current_user(&ctx, "C-100", "P-100", "alice", "repair", 1000.0));
    assert(add_claim_for_current_user(&ctx, "C-CANCEL", "P-100", "alice", "cancel test",
                                      500.0));
    assert(ctx.policy_count == 1 && ctx.claim_count == 2);
    assert(ctx.claims[0].status == CLAIM_PENDING);
    assert(cancel_claim_for_current_user(&ctx, "C-CANCEL"));
    assert(ctx.claims[1].status == CLAIM_CANCELLED);
    assert(!cancel_claim_for_current_user(&ctx, "C-CANCEL"));
    /* 普通用户无权审核或结案；管理员必须先审核通过再结案。 */
    assert(!review_claim_for_current_user(&ctx, "C-100", 1));
    assert(!settle_claim_for_current_user(&ctx, "C-100"));
    assert(login_user_account(&ctx, "admin", "123456"));
    /* 管理员负责审核和结案，不能撤销用户提交的待审核理赔。 */
    assert(!cancel_claim_for_current_user(&ctx, "C-100"));
    assert(!review_claim_for_current_user(&ctx, "C-CANCEL", 1));
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
           loaded.claim_count == 2);
    assert(loaded.claims[0].status == CLAIM_SETTLED);
    assert(loaded.claims[1].status == CLAIM_CANCELLED);
    assert(strlen(loaded.users[0].identity_salt) == 32);
    assert(strlen(loaded.users[0].identity_hash) == 64);
    assert(login_user_account(&loaded, "alice", "new-pass"));

    /* 删除车辆必须同时删除引用它的保单和理赔。 */
    assert(remove_car_for_current_user(&ctx, "A-100"));
    assert(ctx.car_count == 0 && ctx.policy_count == 0 && ctx.claim_count == 0);

    assert(add_car_for_current_user(&ctx, "A-200", "Brand", "Model", "alice",
                                    VEHICLE_TYPE_MOTORCYCLE, VIOLATION_MINOR, purchase_date,
                                    80000.0));
    assert(remove_user_cascade(&ctx, "alice"));
    assert(!user_exists(&ctx, "alice"));
    assert(ctx.car_count == 0 && ctx.policy_count == 0 && ctx.claim_count == 0);

    /* save_users 必须全量重写；再次加载后不能重新出现已删除的 alice。 */
    assert(save_users(&ctx));
    init_app(&loaded);
    assert(load_users(&loaded));
    assert(loaded.user_count == 1 && strcmp(loaded.users[0].username, "bob") == 0);

    /* 上一版九字段记录没有车型，加载时应默认解释为小型车。 */
    legacy_file = fopen("cars.txt", "w");
    assert(legacy_file);
    fputs("OLD-2 Brand Model bob 0 2024 1 1 80000.00\n", legacy_file);
    fclose(legacy_file);
    init_app(&loaded);
    assert(load_cars(&loaded));
    assert(loaded.car_count == 1 && loaded.cars[0].vehicle_type == VEHICLE_TYPE_SMALL_CAR);

    /* 最早版 cars.txt 只有四个字符串；缺失字段也应使用兼容默认值。 */
    legacy_file = fopen("cars.txt", "w");
    assert(legacy_file);
    fputs("OLD-1 Brand Model bob\n", legacy_file);
    fclose(legacy_file);
    init_app(&loaded);
    assert(load_cars(&loaded));
    assert(loaded.car_count == 1 && strcmp(loaded.cars[0].plate, "OLD-1") == 0);
    assert(loaded.cars[0].purchase_price == 0.0 && loaded.cars[0].purchase_date.year == 0);
    assert(loaded.cars[0].vehicle_type == VEHICLE_TYPE_SMALL_CAR);

    assert(date_days_in_month(2024, 2) == 29);
    assert(date_is_valid(&purchase_date));
    assert(normalize_plate("b-d12345", normalized_plate, sizeof(normalized_plate)));
    assert(strcmp(normalized_plate, "B-D12345") == 0);
    assert(validate_plate("A-12345"));
    assert(!validate_plate("1-12345"));
    assert(!validate_plate("A-O1234"));
    assert(!validate_plate("A-12"));
    assert(!validate_plate("INVALID@"));
    puts("Business and persistence smoke tests passed.");
    return 0;
}
