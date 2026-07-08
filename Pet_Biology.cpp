#include <Arduino.h>
#include <ESP32Servo.h>
#include "Config.h"
#include "Pet_Biology.h"

// 创建一个舵机对象
Servo tailServo;

// 触摸感应阈值 (ESP32 的 touchRead 数值在未触摸时通常为 50-70，被触摸时会骤降到 20 以下)
// 比赛实测时，请根据毛绒玩具的厚度，调整这个阈值
const int TOUCH_THRESHOLD = 30; 

// ==========================================
// 初始化宠物生物器官
// ==========================================
void Init_Pet() {
    // 1. 初始化舵机（将尾巴连接到 Config.h 定义的引脚）
    tailServo.attach(PIN_SERVO_TAIL);
    tailServo.write(90); // 尾巴初始放在正中间 (90度)
    
    // 2. 初始化电容触摸（ESP32 原生支持，不需要额外的 pinMode 设置，直接读即可）
    
    Serial.println("[Pet] 🐾 宠物器官(触摸/尾巴)初始化完成.");
}

// ==========================================
// 检查是否被主人抚摸了脑袋
// 返回值：被摸了返回 true，没摸返回 false
// ==========================================
bool Check_Touch() {
    // 强制将触摸引脚设置为输入模式
    pinMode(PIN_TOUCH, INPUT_PULLUP);
    
    // 如果引脚电平被拉低，说明有人触摸 (取决于你的铜线连接方式)
    if (digitalRead(PIN_TOUCH) == LOW) {
        return true;
    }
    return false;
}

// ==========================================
// 开心地摇尾巴动作
// ==========================================
void Wag_Tail() {
    Serial.println("[Pet] 🐕 开始摇尾巴！");
    
    // 来回摇摆 3 次
    for (int i = 0; i < 3; i++) {
        tailServo.write(60); // 偏左
        delay(150);
        tailServo.write(120); // 偏右
        delay(150);
    }
    
    // 摇完恢复居中
    tailServo.write(90); 
}