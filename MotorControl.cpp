// src/MotorControl.cpp
#include <Arduino.h>
#include "MotorControl.h"
#include "Config.h" // 引入全局配置，获取电机引脚号

// ==========================================
// 函数：初始化电机引脚
// 作用：在主程序 setup() 阶段被调用，将定义好的引脚设置为输出模式
// ==========================================
void Init_Motor() {
    // 设置左右轮的4个控制引脚为输出模式 (OUTPUT)
    pinMode(PIN_MOTOR_LEFT_F, OUTPUT);
    pinMode(PIN_MOTOR_LEFT_B, OUTPUT);
    pinMode(PIN_MOTOR_RIGHT_F, OUTPUT);
    pinMode(PIN_MOTOR_RIGHT_B, OUTPUT);
    
    // 初始化时确保电机是停止状态，防止上电乱跑
    StopMotor();
    
    Serial.println("[Motor] 底盘电机初始化完成.");
}

// ==========================================
// 函数：控制小车前进
// 逻辑：左轮正转，右轮正转
// ==========================================
void MoveForward() {
    // 左轮向前
    digitalWrite(PIN_MOTOR_LEFT_F, HIGH);
    digitalWrite(PIN_MOTOR_LEFT_B, LOW);
    // 右轮向前
    digitalWrite(PIN_MOTOR_RIGHT_F, HIGH);
    digitalWrite(PIN_MOTOR_RIGHT_B, LOW);
    
    Serial.println("[Motor] 执行动作：前进 ⬆️");
}

// ==========================================
// 函数：控制小车后退
// 逻辑：左轮反转，右轮反转
// ==========================================
void MoveBackward() {
    // 左轮向后
    digitalWrite(PIN_MOTOR_LEFT_F, LOW);
    digitalWrite(PIN_MOTOR_LEFT_B, HIGH);
    // 右轮向后
    digitalWrite(PIN_MOTOR_RIGHT_F, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_B, HIGH);
    
    Serial.println("[Motor] 执行动作：后退 ⬇️");
}

// ==========================================
// 函数：控制小车左转 (原地打转)
// 逻辑：左轮反转，右轮正转
// ==========================================
void TurnLeft() {
    digitalWrite(PIN_MOTOR_LEFT_F, LOW);
    digitalWrite(PIN_MOTOR_LEFT_B, HIGH);
    
    digitalWrite(PIN_MOTOR_RIGHT_F, HIGH);
    digitalWrite(PIN_MOTOR_RIGHT_B, LOW);
    
    Serial.println("[Motor] 执行动作：左转 ⬅️");
}

// ==========================================
// 函数：控制小车右转 (原地打转)
// 逻辑：左轮正转，右轮反转
// ==========================================
void TurnRight() {
    digitalWrite(PIN_MOTOR_LEFT_F, HIGH);
    digitalWrite(PIN_MOTOR_LEFT_B, LOW);
    
    digitalWrite(PIN_MOTOR_RIGHT_F, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_B, HIGH);
    
    Serial.println("[Motor] 执行动作：右转 ➡️");
}

// ==========================================
// 函数：紧急停止
// 逻辑：所有引脚输出低电平，切断电机动力
// ==========================================
void StopMotor() {
    digitalWrite(PIN_MOTOR_LEFT_F, LOW);
    digitalWrite(PIN_MOTOR_LEFT_B, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_F, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_B, LOW);
    
    Serial.println("[Motor] 执行动作：停止 🛑");
}