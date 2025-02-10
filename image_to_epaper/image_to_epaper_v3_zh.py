#!/usr/bin/python3
# -*- coding:utf-8 -*-

import sys
import os
import logging

# 設定路徑與上傳目錄
picdir = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'pic')
libdir = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'lib')
if os.path.exists(libdir):
    sys.path.append(libdir)

from flask import Flask, request, render_template_string
from werkzeug.utils import secure_filename
from PIL import Image, ImageEnhance
from waveshare_epd import epd4in0e
import numpy as np
from threading import Thread, Lock
import numba

uploads = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'uploads')
UPLOAD_FOLDER = os.path.expanduser(uploads)
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'bmp'}

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
logging.basicConfig(level=logging.DEBUG)

def allowed_file(filename):
    """檢查檔案副檔名是否允許上傳"""
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

# 建立一個全域鎖，保護與電子紙操作相關的程式碼
epd_lock = Lock()

# 將純數值運算部分抽離到一個 numba nopython 函數中
@numba.jit(cache=True)
def dither_loop(arr, palette):
    height, width, channels = arr.shape
    for y in range(height):
        for x in range(width):
            # 取得當前像素的 RGB 值
            old_r = arr[y, x, 0]
            old_g = arr[y, x, 1]
            old_b = arr[y, x, 2]
            # 尋找距離最近的顏色
            best_idx = 0
            best_dist = (old_r - palette[0, 0])**2 + (old_g - palette[0, 1])**2 + (old_b - palette[0, 2])**2
            for i in range(1, palette.shape[0]):
                d0 = old_r - palette[i, 0]
                d1 = old_g - palette[i, 1]
                d2 = old_b - palette[i, 2]
                dist = d0 * d0 + d1 * d1 + d2 * d2
                if dist < best_dist:
                    best_dist = dist
                    best_idx = i
            # 得到新的像素值
            new_r = palette[best_idx, 0]
            new_g = palette[best_idx, 1]
            new_b = palette[best_idx, 2]
            # 計算誤差
            err_r = old_r - new_r
            err_g = old_g - new_g
            err_b = old_b - new_b

            # 更新當前像素值
            arr[y, x, 0] = new_r
            arr[y, x, 1] = new_g
            arr[y, x, 2] = new_b

            # 誤差分配
            if x + 1 < width:
                arr[y, x+1, 0] += err_r * 0.4375
                arr[y, x+1, 1] += err_g * 0.4375
                arr[y, x+1, 2] += err_b * 0.4375
            if y + 1 < height:
                arr[y+1, x, 0] += err_r * 0.3125
                arr[y+1, x, 1] += err_g * 0.3125
                arr[y+1, x, 2] += err_b * 0.3125
                if x - 1 >= 0:
                    arr[y+1, x-1, 0] += err_r * 0.1875
                    arr[y+1, x-1, 1] += err_g * 0.1875
                    arr[y+1, x-1, 2] += err_b * 0.1875
                if x + 1 < width:
                    arr[y+1, x+1, 0] += err_r * 0.0625
                    arr[y+1, x+1, 1] += err_g * 0.0625
                    arr[y+1, x+1, 2] += err_b * 0.0625
    return arr

def floyd_steinberg_dither(image):
    """
    將 image 轉換為僅含下列六種墨水顏色：
      - 黑   : (0, 0, 0)
      - 白   : (255, 255, 255)
      - 黃   : (255, 243, 56)
      - 紅   : (191, 0, 0)
      - 藍   : (100, 64, 255)
      - 綠   : (67, 138, 28)
    """
    # 確保 image 為 RGB 模式（這部分 numba 不支援，故保留在外層）
    if image.mode != "RGB":
        image = image.convert("RGB")
    # 轉換成 numpy 陣列，進行數值運算
    arr = np.array(image, dtype=np.float32)
    palette = np.array([
        [0, 0, 0],           # 黑
        [255, 255, 255],     # 白
        [255, 243, 56],      # 黃
        [191, 0, 0],         # 紅
        [100, 64, 255],      # 藍
        [67, 138, 28]        # 綠
    ], dtype=np.float32)
    arr = dither_loop(arr, palette)
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(arr)

def convert_image_to_6color(image):
    """將影像以 Floyd–Steinberg 誤差擴散轉換為六色"""
    return floyd_steinberg_dither(image)

def process_image(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    讀取並處理上傳圖片，包括自動旋轉、尺寸調整、色彩增強與抖色。
    """
    try:
        image = Image.open(file_path)
    except Exception as e:
        logging.error("Error opening image: " + str(e))
        return None

    orig_width, orig_height = image.size
    target_width, target_height = 600, 400

    # 自動匹配長短邊：若原圖為縱向且目標為橫向，則自動旋轉 90°
    if orig_width < orig_height and target_width > target_height:
        image = image.transpose(Image.ROTATE_90)
        logging.info("Auto-rotated 90° to match landscape display")

    # 保持原比例縮放，將影像置中於 600x400 的黑底畫布
    resized = image.copy()
    resized.thumbnail((target_width, target_height), Image.ANTIALIAS)
    new_image = Image.new("RGB", (target_width, target_height), (0, 0, 0))
    paste_x = (target_width - resized.width) // 2
    paste_y = (target_height - resized.height) // 2
    new_image.paste(resized, (paste_x, paste_y))
    image = new_image

    # 額外旋轉（非 "auto" 或 "0" 時）
    if rotation_choice not in ("auto", "0"):
        try:
            angle = int(rotation_choice)
            image = image.rotate(-angle, expand=True)
            logging.info("Extra rotated {}°".format(angle))
        except Exception as ex:
            logging.error("Error in rotation conversion: {}".format(ex))

    # 色彩增強：飽和度、對比度與亮度
    image = ImageEnhance.Color(image).enhance(sat_factor)
    image = ImageEnhance.Contrast(image).enhance(con_factor)
    image = ImageEnhance.Brightness(image).enhance(bright_factor)

    processed = convert_image_to_6color(image)
    return processed

def clear_display_thread():
    """
    線程函式：初始化電子紙並清除畫面。
    """
    try:
        with epd_lock:
            epd = epd4in0e.EPD()
            logging.info("Thread: Initializing and clearing E-Paper")
            epd.init()
            epd.Clear()
            epd.sleep()
    except Exception as e:
        logging.error("Error in clear_display_thread: " + str(e))

def update_display_thread(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    try:
        with epd_lock:
            logging.info("Thread: Initializing E-Paper for update")
            processed_image = process_image(file_path, rotation_choice, sat_factor, con_factor, bright_factor)
            if processed_image is None:
                logging.error("Processed image is None. Aborting update.")
                return
            epd = epd4in0e.EPD()
            epd.init()  # 新增初始化呼叫
            buffer = epd.getbuffer(processed_image)
            epd.display(buffer)
            logging.info("Thread: Image updated on E-Paper")
            epd.sleep()
    except Exception as e:
        logging.error("Error in update_display_thread: " + str(e))


def process_and_display(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    分別啟動兩個線程進行清除畫面與圖片處理更新。
    """
    try:
        t_update = Thread(target=update_display_thread, args=(file_path, rotation_choice, sat_factor, con_factor, bright_factor))
        t_update.start()
        t_clear = Thread(target=clear_display_thread)
        t_clear.start()
    except Exception as e:
        logging.error("Error in process_and_display: " + str(e))

# 網頁前端 HTML
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
    <p>選擇一張照片並且設定圖片參數</p>
    <p>圖片的處理需要等待一分鐘，請點擊一次按鈕就好</p>
    <p class="param-note">
      旋轉圖片: auto (自動匹配方向) or 0°, 90°, 180°, 270°<br>
      色彩飽和度: 參數範圍推薦 0.0 ~ 3.0 (default 1.5)<br>
      對比度: 參數範圍推薦 0.0 ~ 3.0 (default 1.3)<br>
      亮度: 參數範圍推薦 0.0 ~ 3.0 (default 1.0)
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
      <label for="saturation">色彩飽和度:</label>
      <input type="text" name="saturation" id="saturation" value="1.5"><br>
      <label for="contrast">對比度:</label>
      <input type="text" name="contrast" id="contrast" value="1.3"><br>
      <label for="brightness">亮度:</label>
      <input type="text" name="brightness" id="brightness" value="1.0"><br>
      <div class="button-group">
        <input type="submit" name="action" value="上傳並顯示">
        <input type="submit" name="action" value="清除電子紙畫面">
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
        if action == "清除電子紙畫面":
            try:
                with epd_lock:
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
        else:
            # 確保檔案已上傳
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
                try:
                    file.save(save_path)
                    logging.info("File saved: {}".format(save_path))
                except Exception as e:
                    message = "Error saving file: " + str(e)
                    logging.error(message)
                    return render_template_string(UPLOAD_PAGE, message=message)
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
                message = "正在上傳圖片與顯示中..."
    return render_template_string(UPLOAD_PAGE, message=message)

@app.route('/easter_egg', methods=['GET'])
def cat():
    try:
        with epd_lock:
            epd = epd4in0e.EPD()
            epd.init()
            epd.Clear()
            cat_path = os.path.join(uploads, "cat.jpg")
            rotation_choice = "auto"
            sat_factor = 1.5
            con_factor = 1.3
            bright_factor = 1.0
            processed_image = process_image(cat_path, rotation_choice, sat_factor, con_factor, bright_factor)
            if processed_image is not None:
                buffer = epd.getbuffer(processed_image)
                epd.display(buffer)
                epd.sleep()
    except Exception as e:
        logging.error("Error in easter_egg: " + str(e))

if __name__ == '__main__':
    # from PIL import Image
    # dummy_image = Image.fromarray(np.zeros((10, 10, 3), dtype=np.uint8))
    # floyd_steinberg_dither(dummy_image)

    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    # 注意：由於使用了 @numba.njit(cache=True)，重啟後快取通常會保留，
    # 所以不一定需要提前進行 warm-up 呼叫。
    app.run(host='0.0.0.0', port=5000)
