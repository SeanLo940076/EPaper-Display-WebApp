#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <filesystem>

#include "image_processing.h"
#include "mongoose.h"
#include "server.h"

#if defined(USE_7_3_EPAPER)
    #include "EPD_7in3_epaper.h"
#elif defined(USE_4_0_EPAPER)
    #include "EPD_4in0_epaper.h"
#else
    #include "EPD_4in0_epaper.h"
#endif

// 取得執行檔所在目錄
std::string getExecutablePath() {
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count != -1) {
      return std::string(dirname(path));
  }
  return ".";
}

// 配合前面獲得的目錄指到圖片上傳位置
std::string getUploadDir() {
  std::filesystem::path execPath = getExecutablePath();
  std::filesystem::path uploadPath = execPath.parent_path() / "uploads/";

  if (!std::filesystem::exists(uploadPath)) {
      std::filesystem::create_directories(uploadPath);
  }

  return uploadPath.string();
}


static const char *s_listening_address = "http://0.0.0.0:8080";

static const char *HTML_PAGE = R"(
  <!doctype html>
  <html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>E-Paper Upload</title>
    <style>
      /* 基本重置與 box-sizing 設定 */
      * {
          box-sizing: border-box;
          margin: 0;
          padding: 0;
      }
      body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background: #f2f2f2;
          display: flex;
          justify-content: center;
          align-items: center;
          min-height: 100vh;
          padding: 10px;
      }
      .container {
          width: 100%;
          max-width: 500px;
          background: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 4px 8px rgba(0,0,0,0.1);
      }
      h1 {
          text-align: center;
          color: #333;
          margin-bottom: 15px;
          font-size: 1.5em;
      }
      label {
          display: block;
          margin: 10px 0 5px;
          font-weight: bold;
          color: #444;
      }
      input[type="file"],
      select,
      input[type="text"] {
          width: 100%;
          padding: 10px;
          margin-bottom: 10px;
          border: 1px solid #ccc;
          border-radius: 4px;
          font-size: 1em;
      }
      /* 將 Adaptive Histogram Equalization 與 checkbox 放在同一行 */
      .checkbox-group {
          display: flex;
          align-items: center;
          margin-bottom: 10px;
      }
      .checkbox-group label {
          margin: 0;
          margin-right: 10px;
          font-weight: normal;
      }
      .button-group {
          display: flex;
          flex-wrap: wrap;
          gap: 10px;
          margin-top: 15px;
      }
      .button-group input {
          flex: 1 1 30%;
          background: #007BFF;
          color: #fff;
          border: none;
          padding: 12px;
          border-radius: 4px;
          font-size: 1em;
          cursor: pointer;
          text-align: center;
      }
      .button-group input:hover {
          background: #0056b3;
      }
      .message {
          margin-top: 20px;
          text-align: center;
          font-weight: bold;
          color: green;
      }
      /* Spinner 的 CSS */
      .loader {
          border: 6px solid #f3f3f3;
          border-top: 6px solid #007BFF;
          border-radius: 50%;
          width: 40px;
          height: 40px;
          animation: spin 1s linear infinite;
          margin: 20px auto;
      }
      @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
      }
      /* 小螢幕調整 */
      @media (max-width: 480px) {
          h1 { font-size: 1.3em; }
          .button-group input { font-size: 0.9em; padding: 10px; }
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>E-Paper Image Upload</h1>
      <form method="post" enctype="multipart/form-data" action="/">
        <label for="file">Choose Image:</label>
        <input type="file" name="file" id="file" accept="image/*">
        
        <label for="rotation">Rotation: auto or 0/90/180/270</label>
        <select name="rotation" id="rotation">
          <option value="auto">auto</option>
          <option value="0">0°</option>
          <option value="90">90°</option>
          <option value="180">180°</option>
          <option value="270">270°</option>
        </select>
        
        <label for="saturation">Saturation: 0.0~3.0 (default 1.0)</label>
        <input type="text" name="saturation" id="saturation" value="1.0">
        
        <label for="contrast">Contrast: 0.0~3.0 (default 1.0)</label>
        <input type="text" name="contrast" id="contrast" value="1.0">
        
        <label for="brightness">Brightness: 0.0~3.0 (default 1.0)</label>
        <input type="text" name="brightness" id="brightness" value="1.0">
        
        <div class="checkbox-group">
          <label for="ahe">Adaptive Histogram Equalization</label>
          <input type="checkbox" name="useAHE" id="ahe" value="true">
        </div>
        
        <label for="dither">Dithering Algorithm:</label>
        <select name="dither" id="dither">
          <option value="jarvisJudiceNinke" selected>Jarvis–Judice–Ninke</option>
          <option value="floydSteinberg">Floyd–Steinberg</option>
          <option value="floydSteinbergNoise">Floyd–Steinberg-Noise</option>
        </select>
        
        <div class="button-group">
          <input type="submit" name="action" value="Upload and display">
          <input type="submit" name="action" value="Clear the E-Paper screen">
          <input type="button" id="reset-btn" value="Reset">
        </div>
      </form>
      <div id="spinner" style="display: none;">
        <div class="loader"></div>
      </div>
      <div class="message">%s</div>
    </div>
    <script>
      const form = document.querySelector("form");
      form.addEventListener("submit", function() {
          document.getElementById("spinner").style.display = "block";
      });
      document.getElementById("reset-btn").addEventListener("click", function() {
          window.location.href = "/";
      });
    </script>
  </body>
  </html>
)";


// 回傳上傳頁面
static void send_upload_page(struct mg_connection *c, const std::string &msg) {
    char buf[8192];
    std::snprintf(buf, sizeof(buf), HTML_PAGE, msg.c_str());
    mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", buf);
}

// 處理 multipart/form-data 上傳內容
static void handle_multipart_upload(struct mg_connection *c, struct mg_http_message *hm) {
  size_t ofs = 0, newofs = 0;
  struct mg_http_part part;
  std::string action;
  std::string rotation = "auto";
  float sat = 1.0f, con = 1.0f, bri = 1.0f;
  std::string uploadedFile;
  bool useAHE = false;  // 新增變數，預設不啟用
  std::string ditherMethod = "jarvisJudiceNinke";  // 預設值

  while ((newofs = mg_http_next_multipart(hm->body, ofs, &part)) > ofs) {
      ofs = newofs;
      std::string fieldName(part.name.buf, part.name.len);
      if (fieldName == "action") {
          action.assign(part.body.buf, part.body.len);
      } 
      else if (fieldName == "rotation") {
          rotation.assign(part.body.buf, part.body.len);
      } 
      else if (fieldName == "saturation") {
          try {
              sat = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } 
      else if (fieldName == "contrast") {
          try {
              con = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } 
      else if (fieldName == "brightness") {
          try {
              bri = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } 
      else if (fieldName == "useAHE") {
          std::string val(part.body.buf, part.body.len);
          if(val == "true") {
              useAHE = true;
          }
      } 
      else if (fieldName == "dither") {
          ditherMethod.assign(part.body.buf, part.body.len);
      } 
      else if (fieldName == "file") {
          if (part.filename.len > 0 && part.body.len > 0) {
              std::string uploadDir = getUploadDir();
              uploadedFile = uploadDir + std::string(part.filename.buf, part.filename.len);
              std::ofstream ofs(uploadedFile, std::ios::binary);
              if (ofs.is_open()){
                  ofs.write(part.body.buf, part.body.len);
                  ofs.close();
                  std::cout << "[INFO] File saved to " << uploadedFile << std::endl;
              } 
              else {
                  std::cerr << "[ERROR] cannot open " << uploadedFile << std::endl;
                  uploadedFile.clear();
              }
          }
      }
  }

  std::string msg;
  if (action == "Clear the E-Paper screen") {
      clear_epaper();
      msg = "E-Paper display cleared!";
      std::cout << "[INFO] Clear e-paper" << std::endl;
  } 
  else if (action == "Upload and display") {
      if (!uploadedFile.empty()) {
          // 修改 process_and_display 函數，新增 useAHE 參數
          std::cout << "[INFO] Open " << uploadedFile << std::endl;
          bool ok = process_and_display(uploadedFile, rotation, sat, con, bri, useAHE, ditherMethod);
          msg = ok ? "圖片已上傳並顯示到 e-paper!" : "圖片處理失敗!";
      } 
      else {
          msg = "未找到圖片或圖片名稱太長";
      }
  } 
  else {
      msg = "未知動作或未選擇檔案!";
  }

  send_upload_page(c, msg);
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if(mg_strcmp(hm->uri, mg_str("/"))==0) {
            // GET => 顯示表單
            if(mg_strcmp(hm->method, mg_str("GET"))==0) {
              send_upload_page(c, "");
            } 
            // POST => 處理上傳
            else if(mg_strcmp(hm->method, mg_str("POST"))==0) {
              handle_multipart_upload(c, hm);
            } 
            else {
              mg_http_reply(c,405,"","Method Not Allowed\n");
            }
        } 
        else {
            mg_http_reply(c, 404, "", "Not Found\n");
        }
    }
}

void start_web_server() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    struct mg_connection *conn = mg_http_listen(&mgr, s_listening_address, fn, nullptr);
    if (!conn) {
        std::cerr << "Cannot listen on " << s_listening_address << std::endl;
        return;
    }
    std::cout << "[INFO] Starting server at " << s_listening_address << std::endl;
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);
}
