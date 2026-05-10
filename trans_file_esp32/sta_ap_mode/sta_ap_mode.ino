#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

AsyncWebServer server(80);

String wifiOptions = "";
bool shouldConnect = false;
String reqSSID = "";
String reqPass = "";

int connectStatus = 0; 
String newIP = "";
bool shouldTurnOffAP = false;
unsigned long turnOffTime = 0;

// Giao diện web được cập nhật
const char* fileManagerHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 File Manager</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 600px; margin: auto; padding: 20px; }
    summary { cursor: pointer; background: #e0e0e0; padding: 12px; border-radius: 5px; font-weight: bold; list-style: none; }
    summary::-webkit-details-marker { display: none; }
    #fileList { padding: 10px; background: #f9f9f9; border: 1px solid #ddd; margin-top: 5px; border-radius: 5px; }
    li { margin-bottom: 8px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #eee; padding-bottom: 5px;}
    .del-btn { background: #ff4c4c; color: white; border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px; font-size: 12px;}
  </style>
</head>
<body>
  <h2>He Thong Quan Ly File (.txt)</h2>
  <form method='POST' action='/upload' enctype='multipart/form-data'>
    <input type='file' name='file' accept='.txt'><br><br>
    <input type='submit' value='Tai len'>
  </form>
  <hr>
  
  <details open>
    <summary>📂 Xem danh sach file hien co</summary>
    <div id="fileList">Dang tai du lieu...</div>
  </details>

  <script>
    const maxFiles = 10; 
    
    // Hàm tải danh sách file
    function loadFiles() {
      fetch('/list').then(res => res.json()).then(data => {
        let listDiv = document.getElementById('fileList');
        if(data.length === 0) {
          listDiv.innerHTML = "Khong co file nao trong bo nho.";
          return;
        }
        
        let html = "<ul style='padding-left: 0;'>";
        let limit = Math.min(data.length, maxFiles);
        for(let i = 0; i < limit; i++) {
          html += `<li>
            <span><a href="/download?file=${data[i].name}">${data[i].name}</a> (${data[i].size} bytes)</span>
            <button class='del-btn' onclick="deleteFile('${data[i].name}')">Xoa</button>
          </li>`;
        }
        html += "</ul>";
        
        if(data.length > maxFiles) {
          html += `<p><i>* Đang hiển thị ${maxFiles}/${data.length} file.</i></p>`;
        }
        listDiv.innerHTML = html;
      }).catch(err => {
        document.getElementById('fileList').innerHTML = "Loi khi tai danh sach file.";
      });
    }

    // Hàm gọi API xóa file
    function deleteFile(filename) {
      if(confirm('Ban co chac muon xoa file: ' + filename + '?')) {
        fetch('/delete?file=' + filename, { method: 'DELETE' })
          .then(res => {
            if(res.ok) {
              loadFiles(); // Tải lại danh sách nếu xóa thành công
            } else {
              alert('Xoa that bai!');
            }
          });
      }
    }

    // Khởi chạy khi mở trang
    loadFiles();
  </script>
</body>
</html>
)rawliteral";

void scanWiFi() {
  int n = WiFi.scanNetworks();
  wifiOptions = "";
  for (int i = 0; i < n; ++i) {
    wifiOptions += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("Lỗi LittleFS");
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  scanWiFi();
  WiFi.softAP("ESP32_Setup");
  Serial.print("Đã phát WiFi. Truy cập IP để cấu hình: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (WiFi.status() == WL_CONNECTED && !shouldTurnOffAP) {
      request->send(200, "text/html", fileManagerHTML);
    } else {
      String html = "<h2>Cau hinh WiFi ket noi</h2>"
                    "<form action='/connect' method='POST'>"
                    "Chon WiFi: <select name='ssid'>" + wifiOptions + "</select><br><br>"
                    "Mat khau: <input type='password' name='pass'><br><br>"
                    "<input type='submit' value='Xac nhan'>"
                    "</form>";
      request->send(200, "text/html", html);
    }
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true)) {
      reqSSID = request->getParam("ssid", true)->value();
      reqPass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
      shouldConnect = true;
      connectStatus = 0; 
      request->send(200, "text/html", "<meta http-equiv='refresh' content='3; url=/result'><h2>Dang ket noi vao " + reqSSID + "...</h2>");
    }
  });

  server.on("/result", HTTP_GET, [](AsyncWebServerRequest *request){
    if (connectStatus == 0) {
      request->send(200, "text/html", "<meta http-equiv='refresh' content='2; url=/result'><h2>Van dang xu ly...</h2>");
    } else if (connectStatus == -1) {
      request->send(200, "text/html", "<h2>Ket noi that bai!</h2><a href='/'>Thu lai</a>");
    } else if (connectStatus == 1) {
      String html = "<h2>Thanh cong! Dang chuyen trang...</h2>"
                    "<script>setTimeout(function(){ window.location.href = 'http://" + newIP + "'; }, 5000);</script>";
      request->send(200, "text/html", html);
      shouldTurnOffAP = true;
      turnOffTime = millis();
    }
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<meta http-equiv='refresh' content='2; url=/'>Tai len thanh cong! Dang tai lai...");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    static File file;
    if (!index) file = LittleFS.open("/" + filename, FILE_WRITE);
    if (file) file.write(data, len);
    if (final && file) file.close();
  });

  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) request->send(LittleFS, "/" + request->getParam("file")->value(), "text/plain", true); 
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    bool isFirst = true;
    
    while(file){
      if(!isFirst) json += ",";
      String fileName = String(file.name());
      if(fileName.startsWith("/")) fileName = fileName.substring(1); 
      
      json += "{\"name\":\"" + fileName + "\",\"size\":" + String(file.size()) + "}";
      isFirst = false;
      file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // MỚI: Route xử lý xóa file
  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      String fileName = request->getParam("file")->value();
      if (LittleFS.remove("/" + fileName)) {
        request->send(200, "text/plain", "Da xoa");
      } else {
        request->send(500, "text/plain", "Loi khi xoa file");
      }
    } else {
      request->send(400, "text/plain", "Thieu ten file");
    }
  });

  server.begin();
}

void loop() {
  if (shouldConnect) {
    shouldConnect = false;

    WiFi.disconnect();
    delay(200);
    WiFi.begin(reqSSID.c_str(), reqPass.c_str());

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 30) { 
      delay(500);
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      newIP = WiFi.localIP().toString();
      connectStatus = 1; 
    } else {
      connectStatus = -1; 
    }
  }

  if (shouldTurnOffAP && (millis() - turnOffTime > 3000)) {
    shouldTurnOffAP = false;
    WiFi.softAPdisconnect(true); 
  }
}