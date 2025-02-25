#include <iostream>
#include "EPD_7in3_epaper.h"
 
// 包含 e-Paper 所需的 C 語言標頭檔，注意依照你的目錄結構調整路徑
extern "C" {
  #include "lib/Config/DEV_Config.h"
  #include "lib/e-Paper/EPD_7in3e.h"
  #include "lib/GUI/GUI_Paint.h"
}

bool DEV_init_epaper() {
    if (DEV_Module_Init() != 0) {
        std::cerr << "DEV_Module_Init failed\n";
        return false;
    }
    return true;
}

void init_epaper() {
    EPD_7IN3E_Init();
}

void clear_epaper() {
    EPD_7IN3E_Init();
    EPD_7IN3E_Clear(EPD_7IN3E_WHITE);
    // EPD_7IN3E_Show7Block();
    EPD_7IN3E_Sleep();
}

void display_epaper(UBYTE* imgBuf) {
    EPD_7IN3E_Init();
    EPD_7IN3E_Display(imgBuf);
    EPD_7IN3E_Sleep();
}

void cleanup_epaper() {
    EPD_7IN3E_Sleep();
    DEV_Module_Exit();
}
