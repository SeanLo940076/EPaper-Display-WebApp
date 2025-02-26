#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <string>

extern "C" {
    #include "lib/GUI/GUI_Paint.h"
  }

// 核心函式：讀取圖檔、依參數做圖像增強、dithering，再將結果輸出到 e-Paper
UBYTE* image_process(const std::string &path,
                         const std::string &rotationStr,
                         float sat, float con, float bri, bool useAHE,
                         const std::string &ditherMethod);

#endif // IMAGE_PROCESSING_H
