#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "ESPNOW_RX.h"

// 报警标志位实例化
volatile bool flag_FallDetected = false;

// 数据包结构体定义 (必须与发送端完全一致)
typedef struct struct_message {
    int alert_type; 
} struct_message;

struct_message incomingData;

// ==========================================
// 回调函数：当收到暗哨节点(如毫米波雷达端)信号时自动执行
// ==========================================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    memcpy(&incomingData, data, sizeof(incomingData));
    
    Serial.print("[ESP-NOW] 收到暗哨节点的信号！字节数: ");
    Serial.println(len);

    if (incomingData.alert_type == 1) {
        Serial.println("[ESP-NOW] 🚨 触发高危跌倒报警信号！");
        // 将标志位设为 true，Core 0 的死循环侦测到后立刻出动小车
        flag_FallDetected = true; 
    } else {
        Serial.println("[ESP-NOW] 收到常规心跳包，一切正常.");
    }
}

// ==========================================
// 初始化 ESP-NOW
// ==========================================
void Init_ESPNOW() {
    // 必须在 Wi-Fi Station 模式下运行
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ 初始化失败！");
        return;
    }
    
    // 注册接收回调函数
    esp_now_register_recv_cb(OnDataRecv);
    
    Serial.println("[ESP-NOW] 📡 监听模块初始化完成，正在后台坚守岗位...");
}