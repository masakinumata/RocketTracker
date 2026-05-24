#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// --- 設定値 ---
#define WIFI_CHANNEL 1  // 親機と同じチャンネルに固定
#define NODE_ID 1       // 複数の子機を区別するためのID

// ブザーを接続するピンの定義
#define BUZZER_PIN 10

// ブザーの状態とタイマー管理用の変数
bool isBuzzerOn = false;
unsigned long previousMillis = 0;
const long interval = 150; // 音の切り替え間隔（150ミリ秒）
bool highTone = false;     // 3000Hzと4000Hzを切り替えるフラグ

// 送信先（親機）のMACアドレス
// ※最初はブロードキャスト（すべてFF）にしておくとMACアドレスを調べずにテストできます。
// ※実運用の際は、親機の実際のMACアドレス（例: {0x24, 0x0A, 0xC4, ...}）に変更してください。
uint8_t parentAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- 送信するデータの構造体 ---
// ※親機側と全く同じ構造、同じデータ型（アライメント）にする必要があります
typedef struct struct_message {
    uint8_t id;
    uint32_t counter;   // パケットロス確認用
    float value;   // 任意のデータ（バッテリー電圧など）
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

unsigned long time_ms = 0;
unsigned long previous_time = 0;

// --- 送信完了時のコールバック関数 ---
void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    Serial.print("送信ステータス: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "成功" : "失敗");
}

// --- 受信完了時のコールバック関数　---
void OnCommandRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len == 1) { // 1バイトのデータか確認
        uint8_t cmd = data[0];
        
        if (cmd == 1) {
            isBuzzerOn = true;
            Serial.println("ブザーON");
        } else if (cmd == 0) {
            isBuzzerOn = false;
            Serial.println("ブザーOFF");
            noTone(BUZZER_PIN);
        }
    }
}

void Buzzer(){
}

void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // 初期状態はOFF
    Serial.begin(115200);
    
    // Wi-FiをAP+STAモードに設定
    WiFi.mode(WIFI_AP_STA);
    
    // LR（Long Range）モードに設定
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);

    // 【重要】Wi-Fiチャンネルを親機と強制的に一致させる
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    // ESP-NOWの初期化
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOWの初期化に失敗しました");
        return;
    }

    // 送信コールバックの登録
    esp_now_register_send_cb(OnDataSent);

    esp_now_register_recv_cb(OnCommandRecv);

    // ピア（親機）の情報を登録
    memcpy(peerInfo.peer_addr, parentAddress, 6);
    peerInfo.channel = WIFI_CHANNEL;  
    peerInfo.encrypt = false; // 暗号化なし

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("ピア（親機）の登録に失敗しました");
        return;
    }

    // 構造体の初期化
    myData.id = NODE_ID;
    myData.counter = 0;
}

void loop() {
    time_ms = millis();
    if (time_ms - previous_time >= 1000) {
    // 送信するデータを更新
    myData.counter++;
    myData.value = random(300, 420) / 100.0; // 例: 3.00V〜4.20Vのダミー電圧

    // ESP-NOWで送信
    esp_err_t send_result = esp_now_send(parentAddress, (uint8_t *) &myData, sizeof(myData));
    
    if (send_result == ESP_OK) {
        Serial.printf("データ送信要求成功 (Packet: %d)\n", myData.counter);
    } else {
        Serial.println("データ送信要求失敗");
    }
    previous_time = time_ms;
    }
    
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
