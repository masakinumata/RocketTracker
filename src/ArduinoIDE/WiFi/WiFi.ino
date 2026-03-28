#include <WiFi.h>
#include <WebServer.h>

// ブザーを接続するピンの定義（空欄）
const int BUZZER_PIN = 10; 

// ESP32が飛ばすWi-Fi（アクセスポイント）の設定
const char* ssid = "Payload_Tracker";
const char* password = "password123";

// ⭐️IPアドレスの固定設定
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ブザーの状態とタイマー管理用の変数
bool isBuzzerOn = false;
unsigned long previousMillis = 0;
const long interval = 150; // 音の切り替え間隔（150ミリ秒）
bool highTone = false;     // 3000Hzと4000Hzを切り替えるフラグ

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>トラッカー制御</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; }
    .btn { 
      padding: 20px 40px; 
      font-size: 24px; 
      color: white; 
      border: none; 
      border-radius: 10px; 
      cursor: pointer; 
      margin: 15px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      width: 200px;
    }
    .btn-on { background-color: #ff4c4c; }
    .btn-on:active { background-color: #cc0000; transform: translateY(2px); }
    .btn-off { background-color: #888888; }
    .btn-off:active { background-color: #666666; transform: translateY(2px); }
  </style>
</head>
<body>
  <h2>リカバリートラッカー</h2>
  <button class="btn btn-on" onclick="fetch('/on')">ON</button><br>
  <button class="btn btn-off" onclick="fetch('/off')">OFF</button>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // ⭐️アクセスポイントのIPアドレスを固定（必ず softAP() の前に記述）
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  // アクセスポイント（AP）の起動
  Serial.println("アクセスポイントを起動中...");
  WiFi.softAP(ssid, password);
  
  Serial.print("APのIPアドレス: ");
  Serial.println(WiFi.softAPIP()); // 確実に 192.168.4.1 と出力されます

  // ルーティングの設定
  server.on("/", []() {
    server.send(200, "text/html", html_page);
  });

  // ONボタンが押されたときの処理
  server.on("/on", []() {
    Serial.println("ブザーON");
    isBuzzerOn = true;
    server.send(200, "text/plain", "ON");
  });

  // OFFボタンが押されたときの処理
  server.on("/off", []() {
    Serial.println("ブザーOFF");
    isBuzzerOn = false;
    noTone(BUZZER_PIN); // 音を確実に止める
    server.send(200, "text/plain", "OFF");
  });

  server.begin();
  Serial.println("HTTPサーバーが起動しました");
}

void loop() {
  // クライアントからのリクエストを処理
  server.handleClient();

  // ブザーがONのときの非同期（ノンブロッキング）処理
  if (isBuzzerOn) {
    unsigned long currentMillis = millis();
    
    // 前回の切り替えから150ミリ秒経過したかチェック
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis; // 時間を更新
      
      // 周波数を交互に切り替えて鳴らす
      if (highTone) {
        tone(BUZZER_PIN, 4000);
      } else {
        tone(BUZZER_PIN, 3000);
      }
      highTone = !highTone; // 次回のためにフラグを反転
    }
  }
}