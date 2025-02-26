#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <cstdlib>  // for rand()
#include <opencv2/opencv.hpp>

#include "image_processing.h"
#include "utils.h"

#if defined(USE_7_3_EPAPER)
    #include "EPD_7in3_epaper.h"
#elif defined(USE_4_0_EPAPER)
    #include "EPD_4in0_epaper.h"
#else
    #include "EPD_4in0_epaper.h"
#endif

// 引入 e-Paper 更新相關的函式
extern "C" {
  #include "lib/GUI/GUI_Paint.h"
}

// 調整飽和度：將 BGR 轉換到 HSV，調整 S 分量後再轉回 BGR
static void EnhanceSaturation(cv::Mat &bgr, float sat) {
    if (std::fabs(sat - 1.f) < 1e-6) return;
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    // channels[1] 是飽和度通道，先轉換成浮點數進行計算
    channels[1].convertTo(channels[1], CV_32F);
    channels[1] = channels[1] * sat;
    // 限制在 0 ~ 255 範圍
    cv::min(channels[1], 255, channels[1]);
    channels[1].convertTo(channels[1], CV_8U);
    cv::merge(channels, hsv);
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
}

// 調整亮度
static void EnhanceBrightness(cv::Mat &bgr, float bri) {
    if (std::fabs(bri - 1.f) < 1e-6) return;
    // 利用 convertTo 做乘法調整，內部自動進行飽和處理
    bgr.convertTo(bgr, -1, bri, 0);
}

// 調整對比
static void EnhanceContrast(cv::Mat &bgr, float con) {
    if (std::fabs(con - 1.f) < 1e-6) return;
    // 使用 convertTo 一次完成對比度調整
    bgr.convertTo(bgr, -1, con, 127.5f * (1 - con));
}

// 輔助函式：將誤差乘以係數後加到鄰近像素，並立即夾緊
static inline void addError(cv::Mat &img, int y, int x, const cv::Vec3f &err, float factor) {
    cv::Vec3f pixel = img.at<cv::Vec3f>(y, x) + err * factor;
    for (int c = 0; c < 3; c++) {
        if (pixel[c] < 0.f)
            pixel[c] = 0.f;
        else if (pixel[c] > 255.f)
            pixel[c] = 255.f;
    }
    img.at<cv::Vec3f>(y, x) = pixel;
}

// 改進版本 floydSteinberg ：使用即時夾緊與蛇形掃描
static void floydSteinberg6Color(const cv::Mat &inputBGR, cv::Mat &outIndex) {
    // 轉換原始影像為 32F，並將 BGR 轉成 RGB
    cv::Mat floatRGB;
    inputBGR.convertTo(floatRGB, CV_32FC3);
    cv::cvtColor(floatRGB, floatRGB, cv::COLOR_BGR2RGB);

    outIndex.create(inputBGR.size(), CV_8UC1);
    int rows = floatRGB.rows, cols = floatRGB.cols;

    for (int y = 0; y < rows; y++) {
        // 如果採用蛇形掃描，則奇數行從右往左處理
        bool serpentine = (y % 2 == 1);
        if (!serpentine) {
            for (int x = 0; x < cols; x++) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                int idx = findNearestColorIndex(oldpix);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                // 分配誤差：右邊、正下、左下、右下
                if (x + 1 < cols)
                    addError(floatRGB, y, x + 1, err, 7.f / 16.f);
                if (y + 1 < rows) {
                    addError(floatRGB, y + 1, x, err, 5.f / 16.f);
                    if (x > 0)
                        addError(floatRGB, y + 1, x - 1, err, 3.f / 16.f);
                    if (x + 1 < cols)
                        addError(floatRGB, y + 1, x + 1, err, 1.f / 16.f);
                }
            }
        } else { // 蛇形掃描：從右向左
            for (int x = cols - 1; x >= 0; x--) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                int idx = findNearestColorIndex(oldpix);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                // 分配誤差：左邊、正下、右下、左下（方向與正向鏡像）
                if (x - 1 >= 0)
                    addError(floatRGB, y, x - 1, err, 7.f / 16.f);
                if (y + 1 < rows) {
                    addError(floatRGB, y + 1, x, err, 5.f / 16.f);
                    if (x - 1 >= 0)
                        addError(floatRGB, y + 1, x - 1, err, 1.f / 16.f);
                    if (x + 1 < cols)
                        addError(floatRGB, y + 1, x + 1, err, 3.f / 16.f);
                }
            }
        }
    }
    // 因為我們在每次分配時就即時夾緊，因此可以移除全局夾緊的部分
}

// 擴展版誤差擴散核 (Jarvis–Judice–Ninke)
static void jarvisJudiceNinke6Color(const cv::Mat &inputBGR, cv::Mat &outIndex) {
    // 將原始影像轉換為 32F，並將 BGR 轉為 RGB
    cv::Mat floatRGB;
    inputBGR.convertTo(floatRGB, CV_32FC3);
    cv::cvtColor(floatRGB, floatRGB, cv::COLOR_BGR2RGB);

    outIndex.create(inputBGR.size(), CV_8UC1);
    int rows = floatRGB.rows, cols = floatRGB.cols;

    // 定義 JJN 核（針對左至右掃描）
    struct Kernel { int dx, dy; float weight; };
    const Kernel kernel[12] = {
        {  1, 0, 7.f/48.f },
        {  2, 0, 5.f/48.f },
        { -2, 1, 3.f/48.f },
        { -1, 1, 5.f/48.f },
        {  0, 1, 7.f/48.f },
        {  1, 1, 5.f/48.f },
        {  2, 1, 3.f/48.f },
        { -2, 2, 1.f/48.f },
        { -1, 2, 3.f/48.f },
        {  0, 2, 5.f/48.f },
        {  1, 2, 3.f/48.f },
        {  2, 2, 1.f/48.f }
    };

    // 使用蛇形掃描（偶數行：左到右，奇數行：右到左）
    for (int y = 0; y < rows; y++) {
        bool serpentine = (y % 2 == 1);
        if (!serpentine) {
            for (int x = 0; x < cols; x++) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                int idx = findNearestColorIndex(oldpix);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                // 將誤差依據 JJN 核分配到周圍像素
                for (int k = 0; k < 12; k++) {
                    int nx = x + kernel[k].dx;
                    int ny = y + kernel[k].dy;
                    if (nx >= 0 && nx < cols && ny < rows) {
                        addError(floatRGB, ny, nx, err, kernel[k].weight);
                    }
                }
            }
        } else {
            for (int x = cols - 1; x >= 0; x--) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                int idx = findNearestColorIndex(oldpix);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                // 蛇形掃描時鏡像核：將 dx 乘以 -1
                for (int k = 0; k < 12; k++) {
                    int nx = x - kernel[k].dx; // 相當於 mirror: -dx (鏡像：dx 取反)
                    int ny = y + kernel[k].dy;
                    if (nx >= 0 && nx < cols && ny < rows) {
                        addError(floatRGB, ny, nx, err, kernel[k].weight);
                    }
                }
            }
        }
    }
}

// 混入隨機噪聲的 Floyd–Steinberg-Noise 誤差擴散 (6 色)
static void floydSteinbergWithNoise6Color(const cv::Mat &inputBGR, cv::Mat &outIndex, float noiseMagnitude) {
    // 轉換原始影像為 32F，並將 BGR 轉成 RGB
    cv::Mat floatRGB;
    inputBGR.convertTo(floatRGB, CV_32FC3);
    cv::cvtColor(floatRGB, floatRGB, cv::COLOR_BGR2RGB);

    outIndex.create(inputBGR.size(), CV_8UC1);
    int rows = floatRGB.rows, cols = floatRGB.cols;

    for (int y = 0; y < rows; y++) {
        bool serpentine = (y % 2 == 1);
        if (!serpentine) {
            for (int x = 0; x < cols; x++) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                // 為每個通道加入隨機噪聲：範圍 [-noiseMagnitude, noiseMagnitude]
                cv::Vec3f noise;
                for (int c = 0; c < 3; c++) {
                    noise[c] = ((float)rand() / RAND_MAX * 2.f - 1.f) * noiseMagnitude;
                }
                cv::Vec3f perturbed = oldpix + noise;
                // 夾緊 perturbed 值
                for (int c = 0; c < 3; c++) {
                    if (perturbed[c] < 0.f)
                        perturbed[c] = 0.f;
                    else if (perturbed[c] > 255.f)
                        perturbed[c] = 255.f;
                }
                int idx = findNearestColorIndex(perturbed);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                // 分配誤差使用標準 Floyd–Steinberg 核：
                if (x + 1 < cols)
                    addError(floatRGB, y, x + 1, err, 7.f / 16.f);
                if (y + 1 < rows) {
                    addError(floatRGB, y + 1, x, err, 5.f / 16.f);
                    if (x > 0)
                        addError(floatRGB, y + 1, x - 1, err, 3.f / 16.f);
                    if (x + 1 < cols)
                        addError(floatRGB, y + 1, x + 1, err, 1.f / 16.f);
                }
            }
        } else {
            for (int x = cols - 1; x >= 0; x--) {
                cv::Vec3f oldpix = floatRGB.at<cv::Vec3f>(y, x);
                cv::Vec3f noise;
                for (int c = 0; c < 3; c++) {
                    noise[c] = ((float)rand() / RAND_MAX * 2.f - 1.f) * noiseMagnitude;
                }
                cv::Vec3f perturbed = oldpix + noise;
                for (int c = 0; c < 3; c++) {
                    if (perturbed[c] < 0.f)
                        perturbed[c] = 0.f;
                    else if (perturbed[c] > 255.f)
                        perturbed[c] = 255.f;
                }
                int idx = findNearestColorIndex(perturbed);
                cv::Vec3f newpix = g_paletteRGB[idx];
                outIndex.at<uchar>(y, x) = static_cast<uchar>(idx);
                cv::Vec3f err = oldpix - newpix;
                if (x - 1 >= 0)
                    addError(floatRGB, y, x - 1, err, 7.f / 16.f);
                if (y + 1 < rows) {
                    addError(floatRGB, y + 1, x, err, 5.f / 16.f);
                    if (x - 1 >= 0)
                        addError(floatRGB, y + 1, x - 1, err, 3.f / 16.f);
                    if (x + 1 < cols)
                        addError(floatRGB, y + 1, x + 1, err, 1.f / 16.f);
                }
            }
        }
    }
}

// 自適應直方圖等化 Adaptive Histogram Equalization (CLAHE)
static void AdaptiveHistogramEqualization(cv::Mat &img) {
    // 轉換到 LAB 空間
    cv::Mat lab;
    cv::cvtColor(img, lab, cv::COLOR_BGR2Lab);
    // 分離通道
    std::vector<cv::Mat> lab_channels;
    cv::split(lab, lab_channels);
    // 建立 CLAHE 物件，並設定 clipLimit（可依需求調整）
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(2.0);
    // 對 L 通道執行 CLAHE
    cv::Mat dst;
    clahe->apply(lab_channels[0], dst);
    lab_channels[0] = dst;
    // 合併通道並轉回 BGR
    cv::merge(lab_channels, lab);
    cv::cvtColor(lab, img, cv::COLOR_Lab2BGR);
}

bool process_and_display(const std::string &path,
                         const std::string &rotationStr,
                         float sat, float con, float bri, bool useAHE,
                         const std::string &ditherMethod) {

    // 記錄起始時間
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "[INFO] process_and_display: path=" << path
              << ", rotation=" << rotationStr
              << ", useAHE =" << useAHE << ", sat=" << sat << ", con=" << con << ", bri=" << bri << std::endl;

    // (1) 讀取圖片
    cv::Mat inputBGR = cv::imread(path, cv::IMREAD_COLOR);
    if (inputBGR.empty()){
        std::cerr << "Cannot open " << path << "\n";
        return false;
    }

    // cv::imwrite("pro_image_org.jpg", inputBGR);

    // (2) 旋轉：自動或依使用者指定旋轉
    if (rotationStr == "auto") {
        int w = inputBGR.cols, h = inputBGR.rows;
        if (w < h) {
            cv::rotate(inputBGR, inputBGR, cv::ROTATE_90_CLOCKWISE);
            std::cout << "[INFO] auto-rot => 90 degree" << std::endl;
        }
    } else {
        int angle = std::atoi(rotationStr.c_str());
        if (angle == 90)
            cv::rotate(inputBGR, inputBGR, cv::ROTATE_90_CLOCKWISE);
        else if (angle == 180)
            cv::rotate(inputBGR, inputBGR, 180);
        else if (angle == 270)
            cv::rotate(inputBGR, inputBGR, cv::ROTATE_90_COUNTERCLOCKWISE);
    }

    // cv::imwrite("pro_image_1.jpg", inputBGR);

    // (3.1) 如果啟用 AHE，則執行自適應直方圖均衡化
    if(useAHE) {
        AdaptiveHistogramEqualization(inputBGR);
    }

    // cv::imwrite("pro_image_2.jpg", inputBGR);

    // (3) 調整 飽和度 / 亮度 / 對比度
    EnhanceSaturation(inputBGR, sat);
    EnhanceBrightness(inputBGR, bri);
    EnhanceContrast(inputBGR, con);

    // cv::imwrite("pro_image_3.jpg", inputBGR);

#if defined(USE_7_3_EPAPER)
    const int APP_W = 800, APP_H = 480;
#elif defined(USE_4_0_EPAPER)
    const int APP_W = 600, APP_H = 400;
#else
    const int APP_W = 600, APP_H = 400;
#endif

    // (4) Resize 與 letterbox 到 600×400
    // const int APP_W = 600, APP_H = 400;
    int w = inputBGR.cols, h = inputBGR.rows;
    float scale = std::min(APP_W / (float)w, APP_H / (float)h);
    int newW = static_cast<int>(std::round(w * scale));
    int newH = static_cast<int>(std::round(h * scale));
    cv::Mat resized;
    cv::resize(inputBGR, resized, cv::Size(newW, newH), 0, 0, cv::INTER_AREA);

    cv::Mat letterbox(APP_H, APP_W, CV_8UC3, cv::Scalar(0, 0, 0));
    int offX = (APP_W - newW) / 2;
    int offY = (APP_H - newH) / 2;
    resized.copyTo(letterbox(cv::Rect(offX, offY, newW, newH)));

    // (5) 誤差擴散轉 6 色
    cv::Mat outIndex;
    if(ditherMethod == "floydSteinberg") {
        floydSteinberg6Color(letterbox, outIndex);
    } 
    else if(ditherMethod == "floydSteinbergNoise") {
        floydSteinbergWithNoise6Color(letterbox, outIndex, 2);  // 例如噪聲幅度設定為 2
    } 
    else {  // 預設或 "jarvisJudiceNinke"
        jarvisJudiceNinke6Color(letterbox, outIndex);
    }

    // cv::imwrite("pro_image_4.jpg", outIndex);

    // (6) 旋轉到 e-Paper 實際尺寸 (400×600)
    cv::Mat outIndexRot;

#if defined(USE_7_3_EPAPER)
    // cv::rotate(outIndex, outIndexRot, 180);
    outIndexRot = outIndex;
    const unsigned int epdWidth = 800, epdHeight = 480;
#elif defined(USE_4_0_EPAPER)
    cv::rotate(outIndex, outIndexRot, cv::ROTATE_90_COUNTERCLOCKWISE);
    const unsigned int epdWidth = 400, epdHeight = 600;
#else
    cv::rotate(outIndex, outIndexRot, cv::ROTATE_90_COUNTERCLOCKWISE);
    const unsigned int epdWidth = 400, epdHeight = 600;
#endif

    // (7) 建立 e-Paper 緩衝區
    // const unsigned int epdWidth = 400, epdHeight = 600;
    const unsigned int imgSize = ((epdWidth % 2 == 0) ? (epdWidth / 2) : (epdWidth / 2 + 1)) * epdHeight;
    UBYTE* imgBuf = (UBYTE*)malloc(imgSize);
    if (!imgBuf) {
        std::cerr << "Cannot allocate image buffer!\n";
        return false;
    }
    Paint_NewImage(imgBuf, epdWidth, epdHeight, 0, 0x1);
    Paint_SetScale(6); // 6 色模式
    Paint_SelectImage(imgBuf);
    Paint_SetRotate(ROTATE_180);

    // (8) 將 6 色結果寫入 e-Paper 緩衝區
    for (int r = 0; r < outIndexRot.rows; r++){
        for (int c = 0; c < outIndexRot.cols; c++){
            int idx = outIndexRot.at<uchar>(r, c);
            UBYTE epdColor = indexToEPDColor(idx);
            Paint_SetPixel(c, r, epdColor);
        }
    }

    // 記錄結束時間並輸出花費時間
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "[TIME] Processing took " << duration_ms << " ms" << std::endl;

    // (9) 更新 e-Paper 畫面後進入休眠
    display_epaper(imgBuf);
    free(imgBuf);
    return true;
}
