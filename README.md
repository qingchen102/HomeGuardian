# 🌟 HomeGuardian (Pet Hub)

![Platform](https://img.shields.io/badge/Platform-ESP32--C5-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino_Core_3.x-green)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-orange)

## 📖 项目简介
本项目是“HomeGuardian”零镜头看护系统的核心枢纽端（移动宠物小车）。
系统基于极前沿的 **ESP32-C5 (ESP-SensairShuttle)** 芯片开发，利用 FreeRTOS 实现了严格的双核任务调度。它不仅能通过无感通信（ESP-NOW）实时接收来自沙发/门把手等隐蔽节点的高危跌倒报警并自动寻迹救援，还能通过 Wi-Fi 接入云端大模型（如火山引擎豆包），实现带有温度的智能语音交互。

## ⚙️ 硬件选型
* **主控芯片**: ESP32-C5 (ESP-SensairShuttle RISC-V 架构)
* **底盘驱动**: MX1508 / L298N 直流电机驱动模块
* **通信协议**: ESP-NOW (内网穿透与毫秒级报警) + Wi-Fi / MQTT (云端大模型交互)

## 🧠 核心架构 (双核调度设计)
为保证系统在处理大语言模型 (LLM) 复杂的 JSON 数据时不阻塞底层硬件的急救响应，系统在 `main.cpp` 中进行了严密的 FreeRTOS 物理隔断：
* **Core 0 (高优先级 - 运动与监听任务)**: 负责在后台静默运行 ESP-NOW 回调函数，监听高危报警信号，并直接驱动小车底盘电机进行极速响应。
* **Core 1 (常规优先级 - AI 大脑任务)**: 负责维持长连接 Wi-Fi，调用 HTTP Client 与火山引擎大模型进行多轮对话，并解析生成的 JSON 数据。

## 📂 目录结构与模块说明
本项目采用标准化 C++ 模块化编写，基于 Arduino IDE 2.x 扁平化目录结构：
```text
HomeGuardian_C5/
 ├── HomeGuardian_C5.ino    # 主程序，包含 FreeRTOS 任务调度逻辑
 ├── Config.h               # ⚙️ 全局配置总控 (所有网络、API、引脚均在此修改)
 ├── ESPNOW_RX.h / .cpp     # 📡 无感通信模块 (适配 Core 3.x info 语法)
 ├── LLM_Chat.h / .cpp      # 🤖 云端大模型交互模块 (基于 HTTP POST)
 └── MotorControl.h / .cpp  # 🛞 底盘肌肉控制模块 (底层高低电平/PWM封装)
