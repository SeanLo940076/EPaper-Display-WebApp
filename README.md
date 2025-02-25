# EPaper-Display-WebApp

[中文版本](./README_zh.md)

This project provides a simple mongoose-based web application for uploading images, applying basic adjustments (saturation, contrast, brightness), and displaying the results on a Waveshare e-Paper screen. It also includes a convenient option to clear the e-paper whenever needed.

If you encounter any problems or have suggestions regarding usage, design, or code improvement, feel free to open an [Issue](../../issues) to discuss with us!

---

## Features

- **Image Upload**  
  Upload `.png`, `.jpg`, `.jpeg`, or `.bmp` files via the web interface.

- **Auto Landscape Matching**  
  If the original image is in portrait orientation while the target display is 600×400 (landscape), the system automatically rotates it by 90°.

- **Parameter Adjustments**  
  - **Saturation Factor** (recommended range: `0.0~3.0`, default `1.0`)
  - **Contrast Factor** (recommended range: `0.0~3.0`, default `1.0`)
  - **Brightness Factor** (recommended range: `0.0~3.0`, default `1.0`)

- **Rotation Angles**  
  Choose from `auto, 0, 90, 180, 270`.  
  - `auto`: only automatically rotate if the source is portrait and the target is 600×400 (landscape).  
  - Any other numeric value rotates the image clockwise by the specified degrees.

- **Floyd–Steinberg Dithering**  
  Converts the final image into only six ink colors: black, white, yellow, red, blue, and green.

- **Clear Image**  
  Pressing the **"Clear Image"** button initializes the e-paper and clears the display content.

---

## Demo

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

## Hardware Setup

1. **Hardware**  
   - [Raspberry Pi Zero 2](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/)  
   - [Waveshare 1.3inch UPS HAT](https://www.waveshare.com/wiki/UPS_HAT_(C))  
   - [Waveshare 4inch E-Paper HAT+ (E) Manual](https://www.waveshare.net/wiki/4inch_e-Paper_HAT+_(E)_Manual#Raspberry_Pi)  
   - microSD Card

2. **Operating System**  
   - Recommended: **Raspberry Pi Lite OS**

---

## Assembly Steps

1. **Stack the E-Paper on the Raspberry Pi Zero 2**  
   - Attach the Waveshare E-Paper module to the Pi Zero 2, ensuring the 40-pin connector is aligned properly. Lock it with M2.5 screws if needed.  
2. **Attach the UPS HAT**  
   - Stack the Waveshare 1.3inch UPS HAT onto the Pi Zero 2 with correct power pins alignment, secure with M2.5 screws on each corner.  
3. **Power Test**  
   - Connect a battery to the UPS HAT for charging.  
   - Verify the Pi Zero 2 boots successfully and the screen receives power.

---

## Installation / Usage

Below is an example using **Raspberry Pi Lite OS**, showing basic installation steps:

1. **Update System & Enable SPI**  
   ```bash
   sudo apt update
   sudo apt upgrade -y
   sudo raspi-config
   ```
   - After running `sudo raspi-config` command, the "Interface Options" is shown and you need to enable SPI as the following steps.
     
       ![image](https://gist.github.com/user-attachments/assets/5c3a7f88-060a-41e6-9c71-4c43633636ad)

      ![image](https://gist.github.com/user-attachments/assets/23089a0d-c3b2-401c-b85a-2cb08626df85)

      ![image](https://gist.github.com/user-attachments/assets/0b437788-6cfc-450c-85f9-796ba533a799)

      ![image](https://gist.github.com/user-attachments/assets/ce713ac5-183a-474b-8097-da8b101f738b)

2. **Install Dependencies Waveshare 4inch E-Paper (lg lib)**
   ```bash
   wget https://github.com/joan2937/lg/archive/master.zip
   unzip master.zip
   cd lg-master
   make
   sudo make install
   ```

3. **Clone This Repository**
   ```bash
   git clone https://github.com/SeanLo940076/EPaper-Display-WebApp.git
   ```

4. **Run the Application**
   ```bash
   cd EPaper-Display-WebApp
   mkdir build
   cd build
   cmake ..
   make
   ./ePaper_web
   ```

5. **Enable Startup on Boot (Optional)**
   Edit /etc/rc.local to automatically run the script at startup:

   ```bash
   sudo nano /etc/rc.local
   ```
   在Insert the line before ```exit``` 0:
   > Note: Please change your username
   ```bash
   #!/bin/bash
   # rc.local

   /home/user/EPaper-Display-WebApp/build/ePaper_web > /home/user/EPaper-Display-WebApp/log.txt 2>&1 &
   exit 0
   ```

   Then restart the rc.local service:
   ```bash
   sudo systemctl restart rc-local
   sudo systemctl status rc-local
   ```

6. Access the Web Interface 
   Use a device on the same network and point the browser to http://192.168.0.x:8080 (where x is your Pi Zero 2 IP address).

---

### Project Structure

   ```bash
   ─── EPaper-Display-WebApp
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
   ```

---

### FAQ
1. Cannot upload images?
   - Make sure the file extension is one of png/jpg/jpeg/bmp.
   - Check if uploads/ folder exists and has correct write permissions.

2. Parameters adjusted but results look unexpected?
   - You can fine-tune saturation, contrast, and brightness to find the best visual result.
   - Floyd–Steinberg dithering may produce noticeable grainy patterns, which is normal.

---

### Future Improvements

1. Added 7.3in E-Paper version.
2. It is expected to add the Pomodoro.

### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
We appreciate your feedback and contributions！

