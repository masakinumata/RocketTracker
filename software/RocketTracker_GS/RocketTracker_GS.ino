#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define LED_PIN 48

// --- ESP-NOW 設定 ---
#define WIFI_CHANNEL 1

// --- コマンド送信先（子機）のMACアドレス（ブロードキャストで全子機一斉送信にするのが楽です） ---
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 子機と全く同じ構造体
typedef struct struct_message {
    uint8_t id;
    uint32_t counter;
    float sensor_val;
} struct_message;

// --- BLE 設定 ---
// ※適当なUUIDジェネレーターで作成した固有のID
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define COMMAND_UUID "12345678-1234-5678-1234-56789abcdef0" 

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// --- データ受け渡し用 ---
volatile bool dataUpdated = false;
String blePayload = "";

// --- BLE 接続状態のコールバック ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("スマホとBLE接続しました");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("スマホとのBLE接続が切れました");
        // 切断されたら再度アドバタイズ（発見可能状態）にする
        BLEDevice::startAdvertising();
    }
};

// --- ESP-NOW データ受信時のコールバック ---
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len == sizeof(struct_message)) {
        struct_message myData;
        memcpy(&myData, data, sizeof(myData));

        // RSSIの取得
        int rssi = esp_now_info->rx_ctrl->rssi;

        // スマホ側でパースしやすいように「ID,カウンター,センサー値,RSSI」の文字列にする
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%d,%u,%.2f,%d", 
                 myData.id, myData.counter, myData.sensor_val, rssi);
        
        blePayload = String(buffer);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        dataUpdated = true; // loop() に処理を委譲
    }
}

// --- スマホからBLEで書き込まれた時のコールバック ---
class MyWriteCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCmdCharacteristic) {
        String value = pCmdCharacteristic->getValue();
        
        if (value.length() > 0) {
            char cmd = value[0]; // 最初の1文字を取得
            uint8_t sendData = 0;

            if (cmd == '1') {
                sendData = 1; // ブザーON
                Serial.println("スマホからONを受信");
            } else if (cmd == '0') {
                sendData = 0; // ブザーOFF
                Serial.println("スマホからOFFを受信");
            }

            // ESP-NOWで子機へ転送
            esp_now_send(broadcastAddress, &sendData, sizeof(sendData));
        }
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    // ------------------------------------
    // 1. ESP-NOW (Wi-Fi) のセットアップ
    // ------------------------------------
    WiFi.mode(WIFI_AP_STA);

    // LR（Long Range）モードに設定
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = WIFI_CHANNEL;  
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // ------------------------------------
    // 2. BLE のセットアップ
    // ------------------------------------
    BLEDevice::init("ESP32_Gateway"); // スマホから見えるデバイス名
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Notify（通知）プロパティを持たせたキャラクタリスティックを作成
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    // Notifyを使うための必須ディスクリプタを追加
    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCmdCharacteristic = pService->createCharacteristic(
                                         COMMAND_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
    pCmdCharacteristic->setCallbacks(new MyWriteCallbacks());

    pService->start();

    // アドバタイズ設定
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("BLE Advertising開始。スマホからの接続を待っています...");
}


void loop() {
    // データが更新され、かつスマホがBLE接続されている場合のみ送信
    if (deviceConnected && dataUpdated) {
        dataUpdated = false; // フラグをリセット

        // 送信
        pCharacteristic->setValue(blePayload.c_str());
        pCharacteristic->notify(); // スマホへプッシュ通知

        Serial.print("BLE 送信: ");
        Serial.println(blePayload);
    } else if(deviceConnected && !dataUpdated){

    }

    // 必要に応じて適度なディレイ（Watchdog回避）
    delay(10);
}