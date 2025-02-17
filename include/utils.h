#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <opencv2/opencv.hpp>
#include <cmath>

// 6 色調色盤 (RGB 格式)

// 理論上的顏色
// static const cv::Vec3f g_paletteRGB[6] = {
//   cv::Vec3f(  0.f,   0.f,   0.f),    // black
//   cv::Vec3f(255.f, 255.f, 255.f),    // white
//   cv::Vec3f(255.f, 255.f,  0.f),    // yellow
//   cv::Vec3f(255.f,   0.f,   0.f),    // red
//   cv::Vec3f(  0.f,   0.f, 255.f),    // blue
//   cv::Vec3f(  0.f, 255.f,   0.f)      // green
// };

// // 官方提供的色彩
// static const cv::Vec3f g_paletteRGB[6] = {
//   cv::Vec3f(  0.f,   0.f,   0.f),    // black
//   cv::Vec3f(255.f, 255.f, 255.f),    // white
//   cv::Vec3f(255.f, 243.f,  56.f),    // yellow
//   cv::Vec3f(191.f,   0.f,   0.f),    // red
//   cv::Vec3f(100.f,  64.f, 255.f),    // blue
//   cv::Vec3f( 67.f, 138.f,  28.f)      // green
// };

// 自己調的
static const cv::Vec3f g_paletteRGB[6] = {
  cv::Vec3f( 11.f,  11.f,  15.f),    // black
  cv::Vec3f(247.f, 247.f, 247.f),    // white
  cv::Vec3f(255.f, 230.f,  41.f),    // yellow
  cv::Vec3f(168.f,  27.f,  27.f),    // red
  cv::Vec3f( 29.f,  82.f, 181.f),    // blue
  cv::Vec3f( 60.f, 133.f, 106.f)     // green
  // cv::Vec3f( 67.f, 138.f,  28.f)      // green
};


// 將調色盤索引轉換成 e-Paper 所需的顏色值
inline unsigned char indexToEPDColor(int idx) {
  switch(idx) {
    case 0: return 0x0; // BLACK
    case 1: return 0x1; // WHITE
    case 2: return 0x2; // YELLOW
    case 3: return 0x3; // RED
    case 4: return 0x5; // BLUE
    case 5: return 0x6; // GREEN
    default: return 1;
  }
}

// 找出與輸入 RGB 值最接近的調色盤索引
inline int findNearestColorIndex(const cv::Vec3f &pix) {
  float dr0 = pix[0] - g_paletteRGB[0][0];
  float dg0 = pix[1] - g_paletteRGB[0][1];
  float db0 = pix[2] - g_paletteRGB[0][2];
  float bestSq = dr0 * dr0 + dg0 * dg0 + db0 * db0;
  int best = 0;
  for (int i = 1; i < 6; i++) {
      float dr = pix[0] - g_paletteRGB[i][0];
      float dg = pix[1] - g_paletteRGB[i][1];
      float db = pix[2] - g_paletteRGB[i][2];
      float sqDist = dr * dr + dg * dg + db * db;
      if (sqDist < bestSq) {
          bestSq = sqDist;
          best = i;
      }
  }
  return best;
}

#endif // UTILS_H
