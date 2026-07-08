// ESPNOW_RX.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "ESPNOW_RX.h"

// ==========================================
// 全局标志位：用于通知主程序的 main.cpp 发生了跌倒
// ==========================================
volatile bool flag_FallDetected = false;

// ==========================================
// 数据包结构体定义 (必须和发送端的隐蔽节点保持完全一致)
// ==========================================
typedef struct struct_message {
    int alert_type; 
} struct_message;

// 创建一个变量来存储收到的数据
struct_message incomingData;

// ==========================================
// 回调函数：当 ESP-NOW 接收到数据时自动执行
// (已为你配置为最新的 Core 3.x 语法)
// ==========================================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    memcpy(&incomingData, data, sizeof(incomingData));
    
    Serial.print("[ESP-NOW] 收到隐蔽节点的信号！字节数: ");
    Serial.println(len);

    if (incomingData.alert_type == 1) {
        Serial.println("[ESP-NOW] 🚨 触发高危报警信号！");
        flag_FallDetected = true; 
    } else {
        Serial.println("[ESP-NOW] 收到常规心跳包，一切正常.");
    }
}

// ==========================================
// 函数：初始化 ESP-NOW
// ==========================================
void Init_ESPNOW() {
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ 初始化失败！");
        return;
    }
    
    esp_now_register_recv_cb(OnDataRecv);
    
    Serial.println("[ESP-NOW] 📡 监听模块初始化完成，正在后台坚守岗位...");
}