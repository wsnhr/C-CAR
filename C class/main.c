#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

int main() {
    int choice;

    while (1) {
        system("cls");  

        printf("====== 好车主助手 ======\n");
        printf("1. 用户注册\n");
        printf("2. 用户登录\n");
        printf("3. 注销登录\n");
        printf("0. 退出\n");
        printf("请选择: ");

        scanf("%d", &choice);
        getchar();  // 吃掉回车

        switch (choice) {

        case 1:
            printf("【用户注册】功能待实现...\n");
            break;
        case 2:
            printf("【用户登录】功能待实现...\n");
            break;
        case 3:
            printf("【注销登录】功能待实现...\n");
            break;
        case 0:
            printf("退出系统，再见！\n");
            return 0;
        default:
            printf("无效选择。\n");
        }

        printf("\n按回车键继续...");
		getchar();//我还不知道这是干嘛用的，但它确实能让程序暂停，等待用户按下回车键后继续执行。先别动
    }
}