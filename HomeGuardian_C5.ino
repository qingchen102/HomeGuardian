// src/main.cpp
#include <Arduino.h>
#include <WiFi.h>          
#include "Config.h"
#include "MotorControl.h"
#include "ESPNOW_RX.h"
#include "LLM_Chat.h"

// ==========================================
// 任务1：专职底盘运动与底层报警监听 (运行在 Core 0)
// ==========================================
void Task_MotionAndComm(void *pvParameters) {
    Init_Motor();
    Init_ESPNOW();
    
    Serial.println("[Core 0] 运动与监听任务已启动.");

    while (true) {
        // 检查 ESP-NOW 模块是否收到了沙发节点的报警信号
        if (flag_FallDetected == true) {
            Serial.println("[Core 0] 🚨 收到跌倒报警！立刻前往救援...");
            MoveForward();
            delay(3000); // 模拟小车行驶到目标房间的时间
            StopMotor();
            
            // 清除标志位，防止重复触发
            flag_FallDetected = false; 
        }
        
        // 必须留有延时，防止霸占 CPU 导致看门狗复位重启
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}

// ==========================================
// 任务2：专职 AI 大模型通信与语音交互 (运行在 Core 1)
// ==========================================
void Task_AIChat(void *pvParameters) {
    Init_WiFi();
    
    Serial.println("[Core 1] 云端AI大脑已启动.");

    while (true) {
        // 如果网络断了，尝试重连
        if (WiFi.status() != WL_CONNECTED) {
            Init_WiFi();
        }

        // 这里后续可以加入麦克风的声音检测逻辑
        // 比如如果听到呼救，调用 Ask_LLM("老人呼救了，请生成报警信息")
        
        vTaskDelay(500 / portTICK_PERIOD_MS); 
    }
}

// ==========================================
// 系统初始化 (Setup)
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000); // 给串口监视器一点准备时间
    
    Serial.println("=================================");
    Serial.println("HomeGuardian 双子星 - 宠物中枢启动");
    Serial.println("=================================");

    // 将 任务1 绑定到 核心 0
    xTaskCreatePinnedToCore(
        Task_MotionAndComm,   // 任务函数名
        "Motion_Task",        // 任务别名
        TASK_STACK_SIZE,      // 分配的内存
        NULL,                 // 传递的参数
        2,                    // 优先级 (高优先级，保证响应不卡顿)
        NULL,                 // 任务句柄
        CORE_0                // 指定核心
    );

    // 将 任务2 绑定到 核心 1
    xTaskCreatePinnedToCore(
        Task_AIChat,          // 任务函数名
        "AI_Task",            // 任务别名
        TASK_STACK_SIZE,      // 分配的内存
        NULL,                 // 传递的参数
        1,                    // 优先级 (普通优先级)
        NULL,                 // 任务句柄
        CORE_1                // 指定核心
    );
}

// ==========================================
// 循环 (Loop)
// ==========================================
void loop() {
    // 所有的工作都已经分配给了 FreeRTOS 的两个核心去做了
    // 原生的 loop 相当于主线程，我们直接删掉它以节省资源
    vTaskDelete(NULL); 
}