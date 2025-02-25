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
    #include "EDP_7in3_epaper.h"
#elif defined(USE_4_0_EPAPER)
    #include "EDP_4in0_epaper.h"
#else
    #include "EDP_4in0_epaper.h"
#endif

std::string getExecutablePath() {
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count != -1) {
      return std::string(dirname(path)); // 取得執行檔所在目錄
  }
  return ".";
}

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
    <title>E-Paper Upload</title>
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
      input[type="submit"], input[type="reset"] {
        background: #007BFF; color: #fff; border: none;
        padding: 10px 20px; border-radius: 4px; cursor: pointer;
      }
      input[type="submit"]:hover, input[type="reset"]:hover { background: #0056b3; }
      .message { color: green; font-weight: bold; margin-bottom: 20px; }
      .param-note { font-size: 0.8em; color: #555; }
      a { text-decoration: none; color: #007BFF; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>E-Paper Image Upload</h1>
      <p>選擇一張照片並且設定圖片參數</p>
      <p class="param-note">
        Rotation: auto or 0/90/180/270 <br>
        Saturation: 0.0~3.0 (default 1.0) <br>
        Contrast: 0.0~3.0 (default 1.0) <br>
        Brightness: 0.0~3.0 (default 1.0)
      </p>
      <form method="post" enctype="multipart/form-data" action="/">
        <input type="file" name="file" accept="image/*"><br>
        <label for="rotation">Rotation:</label>
        <select name="rotation" id="rotation">
          <option value="auto">auto</option>
          <option value="0">0°</option>
          <option value="90">90°</option>
          <option value="180">180°</option>
          <option value="270">270°</option>
        </select><br>
        <label>Saturation:</label>
        <input type="text" name="saturation" value="1.0"><br>
        <label>Contrast:</label>
        <input type="text" name="contrast" value="1.0"><br>
        <label>Brightness:</label>
        <input type="text" name="brightness" value="1.0"><br>
        <!-- 新增自適應直方圖均衡化選項 -->
        <label for="ahe">自適應直方圖均衡化:</label>
        <input type="checkbox" name="useAHE" id="ahe" value="true"><br>
        <div class="button-group">
          <input type="submit" name="action" value="上傳並顯示">
          <input type="submit" name="action" value="清除電子紙畫面">
          <!-- 新增 Reset 按鈕，點擊後會重置表單欄位 -->
          <input type="reset" value="Reset">
        </div>
      </form>
      <div class="message">%s</div>
    </div>
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

  while ((newofs = mg_http_next_multipart(hm->body, ofs, &part)) > ofs) {
      ofs = newofs;
      std::string fieldName(part.name.buf, part.name.len);
      if (fieldName == "action") {
          action.assign(part.body.buf, part.body.len);
      } else if (fieldName == "rotation") {
          rotation.assign(part.body.buf, part.body.len);
      } else if (fieldName == "saturation") {
          try {
              sat = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } else if (fieldName == "contrast") {
          try {
              con = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } else if (fieldName == "brightness") {
          try {
              bri = std::stof(std::string(part.body.buf, part.body.len));
          } catch(...) {}
      } else if (fieldName == "useAHE") {
          // 若 checkbox 有被選取，則其值會是 "true"
          std::string val(part.body.buf, part.body.len);
          if(val == "true") {
              useAHE = true;
          }
      } else if (fieldName == "file") {
          if (part.filename.len > 0 && part.body.len > 0) {
              std::string uploadDir = getUploadDir();
              uploadedFile = uploadDir + std::string(part.filename.buf, part.filename.len);
              std::ofstream ofs(uploadedFile, std::ios::binary);
              if (ofs.is_open()){
                  ofs.write(part.body.buf, part.body.len);
                  ofs.close();
                  std::cout << "[INFO] File saved to " << uploadedFile << std::endl;
              } else {
                  std::cerr << "[ERROR] cannot open " << uploadedFile << std::endl;
                  uploadedFile.clear();
              }
          }
      }
  }

  std::string msg;
  if (action == "清除電子紙畫面") {
      clear_epaper();
      msg = "E-Paper display cleared!";
      std::cout << "[INFO] Clear e-paper" << std::endl;
  } else if (action == "上傳並顯示") {
      if (!uploadedFile.empty()) {
          // 修改 process_and_display 函數，新增 useAHE 參數
          std::cout << "[INFO] Open " << uploadedFile << std::endl;
          bool ok = process_and_display(uploadedFile, rotation, sat, con, bri, useAHE);
          msg = ok ? "圖片已上傳並顯示到 e-paper!" : "圖片處理失敗!";
      } else {
          msg = "未找到圖片或圖片名稱太長";
      }
  } else {
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
            } else {
              mg_http_reply(c,405,"","Method Not Allowed\n");
            }
        } else {
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
