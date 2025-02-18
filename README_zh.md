# EPaper-Display-WebApp

[English Version](README.md)

此專案提供了基於 mongoose 的簡易網頁應用程式，支援將使用者上傳的影像進行基本圖像處理，並推送到 Waveshare E-Paper 電子紙上顯示。此外也提供清除圖片按鈕，方便隨時清除電子紙畫面。

如果在使用、設計或程式上遇到任何問題，或有任何改進建議，歡迎提出 [Issue](../../issues) 與我們討論！

---

### 簡要特色

- **上傳圖片**：可透過網頁上傳 `.png`, `.jpg`, `.jpeg`, `.bmp` 檔案。
- **自動長短邊匹配**：若原圖為縱向而目標顯示為橫向(600×400)，系統自動旋轉 90°。
- **參數調整**：
  - 飽和度增強因子 (建議 0.0～3.0，預設 1.0)
  - 對比度增強因子 (建議 0.0～3.0，預設 1.0)
  - 亮度增強因子 (建議 0.0～3.0，預設 1.0)
- **旋轉角度**：可選擇 `auto, 0, 90, 180, 270`，若為 `auto`，則只在原圖縱向時自動旋轉；若為其它值則順時針旋轉對應角度。
- **Jarvis–Judice–Ninke 誤差擴散**：將最終影像轉換為僅含六種墨水顏色（黑、白、黃、紅、藍、綠）。
- **清除圖片**：按下「清除圖片」按鈕後，會初始化電子紙並清空畫面。
---

### Demo

**Hardware Setup / Appearance**  
<div align="center">
  <img src="https://github.com/SeanLo940076/EPaper-Display-WebApp/blob/main/Demo/configuration1.jpg" width="300" />
  <img src="https://github.com/SeanLo940076/EPaper-Display-WebApp/blob/main/Demo/configuration2.jpg" width="300" />
</div>

**E-Paper Display Output**  
<div align="center">
  <img src="https://github.com/SeanLo940076/EPaper-Display-WebApp/blob/main/Demo/Photo1.jpg" width="300" />
  <img src="https://github.com/SeanLo940076/EPaper-Display-WebApp/blob/main/Demo/Photo2.jpg" width="300" />
</div>

**Web page configuration**
<div align="center">
  <img src="https://github.com/SeanLo940076/EPaper-Display-WebApp/blob/main/Demo/configuration3.jpg" width="300" />
</div>
---

### 硬體配置
1. **硬體**：
   - [Raspberry Pi Zero 2](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/)
   - [Waveshare 1.3inch UPS HAT](https://www.waveshare.com/wiki/UPS_HAT_(C))
   - [Waveshare 4inch E-Paper HAT+ (E) Manual](https://www.waveshare.net/wiki/4inch_e-Paper_HAT+_(E)_Manual#Raspberry_Pi)
   - microSD

2. **作業系統**：
   - 建議 **Raspberry Pi Lite OS**

---

### 組裝步驟

1. **將 E-Paper 疊加 Raspberry Pi Zero 2**
   - 先將 **Waveshare E-Paper** 與 **Pi Zero 2** 疊合，確保 40-pin 接腳對應正確，並鎖上 M2.5 銅柱。
2. **疊加 UPS HAT**  
   - 將 **Waveshare 1.3inch UPS HAT** 與 **Pi Zero 2** 疊合，確保供電接角對應正確，並在四角鎖上 M2.5 螺絲。
3. **電源測試**  
   - 接上電池到 **UPS HAT** 充電。  
   - 確認 **Pi Zero 2** 能正常啟動並且螢幕有顯示畫面。

---

### 安裝 / 使用範例

以下以 **Raspberry Pi Lite OS** 為例，示範基礎安裝與拍攝操作：

1. **更新系統並啟用功能**
   ```bash
   sudo apt update
   sudo apt upgrade -y
   sudo raspi-config
   ```
   - 在 `Interface Options` 中啟用 **SPI**

2. **安裝依賴項 Waveshare 4inch E-Paper (lg庫))**
    ```bash
   wget https://github.com/joan2937/lg/archive/master.zip
   unzip master.zip
   cd lg-master
   make
   sudo make install
    ```

3. **Git 拉取檔案**
    ```bash
    git clone https://github.com/SeanLo940076/EPaper-Display-WebApp.git
    ```
4. **執行軟體**
    ```bash
   cd EPaper-Display-WebApp
   mkdir build
   cd build
   cmake ..
   make
    ```

5. **設置開機自啟動**
   編輯 rc.local 文件
   ```bash
   sudo nano /etc/rc.local
   ```

   在 exit 0 之前添加這一行，結果如下
   > 注意：請更換使用者名稱
   ```bash
   #!/bin/bash
   # rc.local

   /home/user/EPaper-Display-WebApp/build/ePaper_web > /home/user/EPaper-Display-WebApp/log.txt 2>&1 &
   exit 0
   ```

   重新啟動 rc.local 服務
   ```bash
   sudo systemctl restart rc-local
   ```

   檢查服務狀態
   ```bash
   sudo systemctl status rc-local
   ```

6. 在同網域下的其他設備開啟 http://192.168.0.x:8080
   > 注意：x 為 Pi zero 2 的 IP 地址

---

### 專案結構
└── EPaper-Display-WebApp
    ├── build
    ├── CMakeLists.txt
    ├── Demo
    ├── include
    │   ├── epaper.h
    │   ├── image_processing.h
    │   ├── server.h
    │   └── utils.h
    ├── lib
    │   ├── Config
    │   ├── e-Paper
    │   ├── Fonts
    │   ├── GUI
    │   └── mongoose
    ├── log.txt
    ├── README.md
    ├── README_zh.md
    ├── src
    │   ├── epaper.cpp
    │   ├── image_processing.cpp
    │   ├── main.cpp
    │   └── server.cpp
    └── uploads
---

### 常見問題
1. 無法上傳圖片？
   請確認檔案副檔名必須為 png/jpg/jpeg/bmp。
   請確認 uploads/ 目錄存在且有寫入權限。

2. 調整參數後顯示結果不如預期？
   依實際觀看效果微調飽和度、對比度與亮度因子。
   Floyd–Steinberg 誤差擴散會讓影像呈現抖色顆粒狀，屬正常現象。

3. 請注意，目前 v1 適用於任何硬體，v2 則僅適用於 Pi zero 2。

---

### 未來改進
1. 預計增加番茄鐘的功能

### License

本專案採用 MIT License 授權。詳細內容請參考 [LICENSE](LICENSE) 檔案。
感謝你的閱讀，祝你使用愉快，期待你的回饋與貢獻！