#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <string>

// 核心函式：讀取圖檔、依參數做圖像增強、dithering，再將結果輸出到 e-Paper
bool process_and_display(const std::string &path,
                         const std::string &rotationStr,
                         float sat, float con, float bri,
                         bool useAHE);

#endif // IMAGE_PROCESSING_H
