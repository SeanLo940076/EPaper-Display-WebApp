#include "epaper.h"
#include "server.h"
#include <cstdlib>
#include <iostream>

int main() {
    // 建立上傳目錄
    // system("mkdir -p uploads");

    // 初始化 e-Paper
    if (!DEV_init_epaper()) {
        std::cerr << "EPaper 初始化失敗！\n";
        return -1;
    }

    // 啟動 Web 伺服器（會阻塞在事件迴圈中）
    start_web_server();

    // 結束前清理 e-Paper
    cleanup_epaper();
    return 0;
}
