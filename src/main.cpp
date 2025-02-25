#include <cstdlib>
#include <iostream>
#include "server.h"

#if defined(USE_7_3_EPAPER)
    #include "EDP_7in3_epaper.h"
#elif defined(USE_4_0_EPAPER)
    #include "EDP_4in0_epaper.h"
#else
    #include "EDP_4in0_epaper.h"
#endif

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
