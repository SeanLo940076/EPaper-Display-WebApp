#ifndef EPAPER_H
#define EPAPER_H

extern "C" {
    #include "lib/e-Paper/EPD_4in0e.h"
  }

// 初始化 e-Paper 模組，若失敗回傳 false
bool DEV_init_epaper();

// Initialize the e-Paper register
void init_epaper();

void display_epaper(UBYTE* imgBuf);

// 清除 e-Paper 畫面（清白後進入休眠）
void clear_epaper();

// 程式結束前的清理動作（進入休眠並釋放模組）
void cleanup_epaper();

#endif // EPAPER_H
