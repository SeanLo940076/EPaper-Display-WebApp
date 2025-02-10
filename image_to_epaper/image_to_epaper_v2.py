#!/usr/bin/python3
# -*- coding:utf-8 -*-

import sys
import os
picdir = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'pic')
libdir = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'lib')
if os.path.exists(libdir):
    sys.path.append(libdir)

import logging
from flask import Flask, request, render_template_string
from werkzeug.utils import secure_filename
from PIL import Image, ImageEnhance
from waveshare_epd import epd4in0e
import numpy as np
from threading import Thread, Lock
# import time

# 設定上傳目錄與允許的檔案格式
uploads = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'uploads')
UPLOAD_FOLDER = os.path.expanduser(uploads)
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'bmp'}

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
logging.basicConfig(level=logging.DEBUG)

def allowed_file(filename):
    """檢查檔案副檔名是否允許上傳"""
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

# ------------------------------
# Floyd–Steinberg 誤差擴散（固定預設係數）
# ------------------------------
def floyd_steinberg_dither(image):
    """
    將 image 轉換為僅含下列六種墨水顏色：來自 waveshare 提供的顏色
      - 黑   : (0, 0, 0)
      - 白   : (255, 255, 255)
      - 黃   : (255, 243, 56)
      - 紅   : (191, 0, 0)
      - 藍   : (100, 64, 255)
      - 綠   : (67, 138, 28)
      
    誤差分配係數固定為：
      - 右側 : 0.4375 (7/16)
      - 下方 : 0.3125 (5/16)
      - 左下 : 0.1875 (3/16)
      - 右下 : 0.0625 (1/16)
    """

    if image.mode != "RGB":
        image = image.convert("RGB")
    arr = np.array(image, dtype=np.float32)
    height, width, _ = arr.shape

    # 定義六色調色盤
    palette = np.array([
        [0, 0, 0],           # 黑
        [255, 255, 255],     # 白
        [255, 243, 56],      # 黃
        [191, 0, 0],         # 紅
        [100, 64, 255],      # 藍
        [67, 138, 28]        # 綠
    ], dtype=np.float32)

    def find_nearest_color(pixel):
        distances = np.sum((palette - pixel) ** 2, axis=1)
        index = np.argmin(distances)
        return palette[index]

    for y in range(height):
        for x in range(width):
            old_pixel = arr[y, x].copy()
            new_pixel = find_nearest_color(old_pixel)
            arr[y, x] = new_pixel
            error = old_pixel - new_pixel
            if x + 1 < width:
                arr[y, x+1] += error * 0.4375
            if y + 1 < height:
                arr[y+1, x] += error * 0.3125
                if x > 0:
                    arr[y+1, x-1] += error * 0.1875
                if x + 1 < width:
                    arr[y+1, x+1] += error * 0.0625
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(arr)

def convert_image_to_6color(image):
    """將影像以 Floyd–Steinberg 誤差擴散轉換為六色"""
    return floyd_steinberg_dither(image)
# ------------------------------
# 結束影像轉換部分
# ------------------------------

def process_image(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    讀取並處理上傳圖片，步驟包括：
      1. 自動匹配長短邊：若原圖為縱向且目標尺寸為 600x400（橫向），則自動旋轉 90°。
      2. 保持原比例縮放，將影像置中於 600x400 的黑底畫布。
      3. 根據使用者選擇額外旋轉（選項："auto"、"0"、"90"、"180"、"270"，其中 "auto" 或 "0" 表示不額外旋轉）。
      4. 色彩增強：調整飽和度 (sat_factor)、對比度 (con_factor) 與亮度 (bright_factor)。
         - 飽和度增強因子建議範圍：0.0 ~ 3.0（預設 1.5）。
         - 對比度增強因子建議範圍：0.0 ~ 3.0（預設 1.3）。
         - 亮度增強因子建議範圍：0.0 ~ 3.0（預設 1.0）。
      5. 最後，使用 Floyd–Steinberg 誤差擴散轉換為六色影像。
    """
    # 計算處理時間起始點
    # start_time = time.time()

    image = Image.open(file_path)
    orig_width, orig_height = image.size
    target_width, target_height = 600, 400

    # 自動匹配長短邊：若原圖為縱向且目標為橫向，則自動旋轉 90°。
    if orig_width < orig_height and target_width > target_height:
        image = image.transpose(Image.ROTATE_90)
        logging.info("Auto-rotated 90° to match landscape display")

    # 保持原比例縮放，置中填充黑色背景
    resized = image.copy()
    # 使用 LANCZOS 取代已棄用的 ANTIALIAS
    resized.thumbnail((target_width, target_height), Image.ANTIALIAS)
    new_image = Image.new("RGB", (target_width, target_height), (0, 0, 0))
    paste_x = (target_width - resized.width) // 2
    paste_y = (target_height - resized.height) // 2
    new_image.paste(resized, (paste_x, paste_y))
    image = new_image

    # 額外旋轉（若 rotation_choice 非 "auto" 或 "0"）
    if rotation_choice not in ("auto", "0"):
        try:
            angle = int(rotation_choice)
            image = image.rotate(-angle, expand=True)
            logging.info("Extra rotated {}°".format(angle))
        except Exception as ex:
            logging.error("Error in rotation conversion: {}".format(ex))

    # 色彩增強：調整飽和度、對比度與亮度
    image = ImageEnhance.Color(image).enhance(sat_factor)
    image = ImageEnhance.Contrast(image).enhance(con_factor)
    image = ImageEnhance.Brightness(image).enhance(bright_factor)

    processed = convert_image_to_6color(image)

    # # 計算並 log 處理總時間
    # end_time = time.time()
    # processing_time = end_time - start_time
    # logging.info("Image processing completed in {:.2f} seconds".format(processing_time))
    return processed

# 建立一個全局鎖，確保與電子紙相關的操作不會同時進行
# epd_lock = Lock()

def clear_display_thread():
    """
    線程函式：初始化電子紙並清除畫面。
    此線程用來提供即時回饋，快速清除現有畫面。
    """
    try:
        # with epd_lock:
        epd = epd4in0e.EPD()
        logging.info("Thread: Initializing and clearing E-Paper")
        epd.init()
        epd.Clear()
        # 清除後先進入睡眠，讓畫面保持清空狀態
        # epd.sleep()
    except Exception as e:
        logging.error("Error in clear_display_thread: " + str(e))

def update_display_thread(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    線程函式：進行圖片處理並推送新畫面到電子紙。
    這部分需要較長時間，完成後會更新顯示內容。
    """
    try:
        # with epd_lock:
        logging.info("Thread: Initializing E-Paper for update")
        # epd.init()
        # 執行圖片處理 (包含自動旋轉、尺寸調整、色彩增強與抖色)
        processed_image = process_image(file_path, rotation_choice, sat_factor, con_factor, bright_factor)
        epd = epd4in0e.EPD()
        buffer = epd.getbuffer(processed_image)
        epd.display(buffer)
        logging.info("Thread: Image updated on E-Paper")
        epd.sleep()
    except Exception as e:
        logging.error("Error in update_display_thread: " + str(e))

def process_and_display(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    分別以兩個線程處理：
      - 一個線程快速初始化電子紙並清除現有畫面 (clear_display_thread)。
      - 另一個線程進行圖片處理並更新顯示 (update_display_thread)。
    """
    try:
        # 建立並啟動圖片處理與顯示的線程
        t_update = Thread(target=update_display_thread, args=(file_path, rotation_choice, sat_factor, con_factor, bright_factor))
        t_update.start()

        # 建立並啟動建清除畫面的線程
        t_clear = Thread(target=clear_display_thread)
        t_clear.start()
        # t_clear.join()
        
        # 若希望等待兩個線程皆完成，可以加入 join()（這會使主線程等待）
        # t_clear.join()
        # t_update.join()
    except Exception as e:
        logging.error("Error in process_and_display: " + str(e))

# ------------------------------
# 更新後的網頁介面：按鈕並排且分開「上傳並顯示」與「清除圖片」
# ------------------------------
UPLOAD_PAGE = '''
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Upload Image to E-Paper</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f7f7f7; text-align: center; }
    .container { width: 90%; max-width: 600px; margin: 30px auto; background: #fff;
                 padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); border-radius: 8px; }
    h1 { color: #333; }
    input[type="file"], select, input[type="text"] {
      margin: 10px 0; padding: 8px; font-size: 1em; width: 80%;
    }
    .button-group {
      display: flex; justify-content: center; gap: 100px; margin-top: 10px;
    }
    input[type="submit"] {
      background: #007BFF; color: #fff; border: none;
      padding: 10px 20px; border-radius: 4px; cursor: pointer;
    }
    input[type="submit"]:hover { background: #0056b3; }
    .message { color: green; font-weight: bold; margin-bottom: 20px; }
    .param-note { font-size: 0.8em; color: #555; }
    a { text-decoration: none; color: #007BFF; }
  </style>
</head>
<body>
  <div class="container">
    <h1>E-Paper Image Upload</h1>
    <p>Please select an image and set the parameters below.</p>
    <p>It will take about 1 minute to refresh the photos.</p>
    <p class="param-note">
      Rotation: auto (match orientation) or 0°, 90°, 180°, 270°<br>
      Saturation Factor: recommended 0.0 ~ 3.0 (default 1.5)<br>
      Contrast Factor: recommended 0.0 ~ 3.0 (default 1.3)<br>
      Brightness Factor: recommended 0.0 ~ 3.0 (default 1.0)
    </p>
    <form method="post" enctype="multipart/form-data" action="/">
      <input type="file" name="file" accept="image/*"><br>
      <label for="rotation">Rotation:</label>
      <select name="rotation" id="rotation">
        <option value="auto">auto (match orientation)</option>
        <option value="0">0°</option>
        <option value="90">90°</option>
        <option value="180">180°</option>
        <option value="270">270°</option>
      </select><br>
      <label for="saturation">Saturation Factor:</label>
      <input type="text" name="saturation" id="saturation" value="1.5"><br>
      <label for="contrast">Contrast Factor:</label>
      <input type="text" name="contrast" id="contrast" value="1.3"><br>
      <label for="brightness">Brightness Factor:</label>
      <input type="text" name="brightness" id="brightness" value="1.0"><br>
      <div class="button-group">
        <input type="submit" name="action" value="Upload and Display">
        <input type="submit" name="action" value="Clear Display">
      </div>
    </form>
    {% if message %}
      <div class="message">{{ message }}</div>
    {% endif %}
    <p><a href="/">Reset</a></p>
  </div>
</body>
</html>
'''

@app.route('/', methods=['GET', 'POST'])
def upload_file():
    message = None
    if request.method == 'POST':
        action = request.form.get("action")
        # 如果按下 "Clear Display"，則進行清除操作
        if action == "Clear Display":
            try:
                epd = epd4in0e.EPD()
                logging.info("Initializing E-Paper to clear display")
                epd.init()
                epd.Clear()
                epd.sleep()
                message = "E-Paper display cleared!"
            except Exception as e:
                message = "Error clearing display: " + str(e)
                logging.error("Error clearing display: " + str(e))
            return render_template_string(UPLOAD_PAGE, message=message)
        # 否則，處理上傳並顯示圖片
        else:
            if 'file' not in request.files:
                message = "No file selected"
                return render_template_string(UPLOAD_PAGE, message=message)
            file = request.files['file']
            if file.filename == '':
                message = "No file selected"
                return render_template_string(UPLOAD_PAGE, message=message)
            if file and allowed_file(file.filename):
                filename = secure_filename(file.filename)
                save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
                file.save(save_path)
                logging.info("File saved: {}".format(save_path))
                rotation_choice = request.form.get("rotation", "auto")
                try:
                    sat_factor = float(request.form.get("saturation", "1.5"))
                except ValueError:
                    sat_factor = 1.5
                try:
                    con_factor = float(request.form.get("contrast", "1.3"))
                except ValueError:
                    con_factor = 1.3
                try:
                    bright_factor = float(request.form.get("brightness", "1.0"))
                except ValueError:
                    bright_factor = 1.0
                process_and_display(save_path, rotation_choice, sat_factor, con_factor, bright_factor)
                message = "The image is uploading and displaying on the E-Paper!"
    return render_template_string(UPLOAD_PAGE, message=message)

@app.route('/easter_egg', methods=['GET'])
def cat():
    try:
        logging.info("This is my cat")
        epd = epd4in0e.EPD()
        epd.init()
        epd.Clear()
        cat_path = os.path.join(uploads, "cat.jpg")
        rotation_choice = "auto"  # 你可以硬編碼參數，或是從參數中獲取
        sat_factor = 1.5
        con_factor = 1.3
        bright_factor = 1.0
        processed_image = process_image(cat_path, rotation_choice, sat_factor, con_factor, bright_factor)
        buffer = epd.getbuffer(processed_image)
        epd.display(buffer)
        epd.sleep()
    except Exception as e:
        logging.error("Error clearing display: " + str(e))

if __name__ == '__main__':
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    app.run(host='0.0.0.0', port=5000)