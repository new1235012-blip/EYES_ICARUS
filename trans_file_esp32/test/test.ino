#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

//wifi
const char *name = "VietThang";
const char *pass = "tram5122008";

WebServer server(80);
File upfile;

const char* htmlTrangChu = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Upload File ESP32</title></head>
<body style="font-family: Arial; padding: 20px;">
  <h2>Truyền 1 File xuống ESP32</h2>
  <form method="POST" action="/upload" enctype="multipart/form-data">
    <input type="file" name="file_cua_ban" required><br><br>
    <input type="submit" value="Tải Lên">
  </form>
</body>
</html>
)rawliteral";


void myupload();
void readfile();

void setup(){
  Serial.begin(115200);
  
  if(!LittleFS.begin(true)){
    Serial.println("error littlefs");
    return;
  }

  //connect wifi
  WiFi.begin(name, pass);
  Serial.print("Connecting wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.print("\nESP32 IP: "); Serial.println(WiFi.localIP());

  //server
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlTrangChu);
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/html", "<h3>Tai len thanh cong!</h3><a href='/'>Quay lai</a>");
  }, myupload);

  server.begin();
  Serial.println("Web Server đã chạy!");
}

void loop(){
  server.handleClient();
}

void myupload(){
  HTTPUpload& upload = server.upload(); 

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename; 
    Serial.print("Upload start..."); Serial.println(filename);
    
    upfile = LittleFS.open(filename, "w");
    if(!upfile){
      Serial.println("Write error");
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    upfile.write(upload.buf, upload.currentSize);
    Serial.printf("Write %d bytes\n", upload.currentSize);

  } else if (upload.status == UPLOAD_FILE_END) {
    upfile.close();
    Serial.printf("Upload End: %d bytes\n", upload.totalSize);

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("Upload Aborted");
  }
}


