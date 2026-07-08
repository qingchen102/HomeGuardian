# 🌟 HomeGuardian
![Platform](https://img.shields.io/badge/Platform-ESP32--C5-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino_Core_3.x-green)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-orange)
![License](https://img.shields.io/badge/License-MIT-brightgreen)

## 📖 项目简介
**HomeGuardian** 是一套专为独居老人设计的“无感、无镜头、高隐私”智能看护系统。
不同于市面上冷冰冰的监控摄像头，我们将其打造为一只**具备全屋环境感知能力、接入云端大模型大脑、且能提供真实物理陪伴的电子生命**。

系统通过“静止暗哨（环境感知）”与“移动中枢（小车宠物）”的双重架构，实现对老人跌倒、异常行为的毫秒级响应，并能主动介入（如提醒服药、物理陪伴）。

## 核心设计理念
* **Privacy First (隐私至上)**：拒绝任何摄像头，采用毫米波雷达/Wi-Fi CSI感知技术，保障老人浴室/卧室尊严。
* **Zero-Wearable (无感化)**：老人无需佩戴任何手环，解决老人健忘、排斥穿戴的问题。
* **Active Companionship (主动陪伴)**：不仅仅是机器人，它是宠物。具备“摸头杀”反馈与“摇尾巴”情感表达，实现养老赛道的技术与温度结合。

## 🧠 系统架构
系统由三层逻辑构成：
1. **神经末梢层 (环境感知)**：静止部署在墙壁/天花板的 ESP32 节点（毫米波雷达/Wi-Fi 感知），通过 ESP-NOW 广播异常信号。
2. **电子生命层 (移动中枢)**：搭载 ESP32-C5 的小车，具备双核调度、主动避障、语音交互与物理服药递送功能。
3. **云端中枢 (情感交互)**：通过火山引擎豆包大模型，赋予小车拟人化的对话能力。

## 📂 工程目录结构
本项目采用高模块化设计，清晰划分了底盘、通信、大模型与生物交互逻辑：

```text
HomeGuardian_C5/
 ├── HomeGuardian_C5.ino    # 主脑：FreeRTOS 多任务调度中心
 ├── Config.h               # 全局总控：引脚配置与 API 密钥
 ├── MotorControl.h/cpp     # 小脑：底盘驱动与超声波避障引擎
 ├── ESPNOW_RX.h/cpp        # 顺风耳：ESP-NOW 毫秒级报警监听
 ├── LLM_Chat.h/cpp         # 大脑：豆包 AI 大模型 HTTP 对接
 └── Pet_Biology.h/cpp      # 灵魂：生物特质模块 (触摸反馈/舵机摇尾)