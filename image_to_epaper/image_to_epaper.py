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
    image = Image.open(file_path)
    orig_width, orig_height = image.size
    target_width, target_height = 600, 400

    # 自動匹配長短邊：若原圖為縱向且目標為橫向，則自動旋轉 90°。
    if orig_width < orig_height and target_width > target_height:
        image = image.transpose(Image.ROTATE_90)
        logging.info("自動旋轉 90 度以匹配橫向顯示")

    # 保持原比例縮放，置中填充黑色背景
    resized = image.copy()
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
            logging.info("額外旋轉 {} 度".format(angle))
        except Exception as ex:
            logging.error("旋轉角度轉換錯誤：{}".format(ex))

    # 色彩增強：調整飽和度、對比度與亮度
    image = ImageEnhance.Color(image).enhance(sat_factor)
    image = ImageEnhance.Contrast(image).enhance(con_factor)
    image = ImageEnhance.Brightness(image).enhance(bright_factor)

    processed = convert_image_to_6color(image)
    return processed

def process_and_display(file_path, rotation_choice, sat_factor, con_factor, bright_factor):
    """
    處理圖片後，初始化電子紙並將結果推送顯示。
    """
    try:
        epd = epd4in0e.EPD()
        logging.info("初始化電子紙並清除畫面")
        epd.init()
        epd.Clear()
        processed_image = process_image(file_path, rotation_choice, sat_factor, con_factor, bright_factor)
        # logging.info("初始化電子紙並清除畫面")
        # epd.init()
        # epd.Clear()
        buffer = epd.getbuffer(processed_image)
        epd.display(buffer)
        logging.info("影像已推送到電子紙")
        epd.sleep()
    except Exception as e:
        logging.error("處理或推送圖片時發生錯誤: {}".format(e))

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
      display: flex; justify-content: center; gap: 20px; margin-top: 10px;
    }
    input[type="submit"] {
      background: #007BFF; color: #fff; border: none; padding: 10px 20px;
      border-radius: 4px; cursor: pointer;
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
    <p class="param-note">
      Rotation: auto (match orientation) or 0°, 90°, 180°, 270°<br>
      Saturation Factor: recommended 0.0 ~ 3.0 (default 1.5)<br>
      Contrast Factor: recommended 0.0 ~ 3.0 (default 1.3)<br>
      Brightness Factor: recommended 0.0 ~ 3.0 (default 1.0)
    </p>
    <!-- 將所有輸入與按鈕放在同一個表單中 -->
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
                message = "Image uploaded and displayed on E-Paper!"
    return render_template_string(UPLOAD_PAGE, message=message)

if __name__ == '__main__':
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    app.run(host='0.0.0.0', port=5000)