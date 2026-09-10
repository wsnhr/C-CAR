#define _CRT_SECURE_NO_WARNINGS
#include "car.h"
#include "gui.h"

/*
 * 程序入口。
 *
 * C 程序从 main 函数开始执行。参数列表写成 void，表示这个程序不接收
 * 命令行参数；返回 int 是因为程序结束时要向操作系统报告退出状态。
 * 返回 0 按惯例表示正常结束，非 0 通常表示发生错误。
 */
int main(void)
{
    /*
     * ctx 是 AppContext 类型的“对象”，保存程序运行期间的全部业务数据。
     * 它现在只是分配了内存，里面的值还不应直接使用。
     */
    AppContext ctx;

    /*
     *init_app 需要 AppContext*（指针），所以传入
     * &ctx 而不是 ctx。函数通过这个地址把所有计数和当前用户初始化。
     */
    init_app(&ctx);

    /*
     * 把同一个数据容器交给 GUI。gui_run 会加载文件、创建窗口并进入事件
     * 循环；在用户关闭程序之前，该函数通常不会返回。
     */
    gui_run(&ctx);

    /* 告诉操作系统：程序正常结束。 */
    return 0;
}
