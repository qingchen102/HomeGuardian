# HomeGuardian - Node C: AI 语音宠物中枢 (AI Pet Hub)

[![Version](https://img.shields.io/badge/Version-1.0.0-blue.svg)]()
[![Platform](https://img.shields.io/badge/Platform-ESP32--C5-orange.svg)]()
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D.svg)]()

> 本模块为 **HomeGuardian** 智能养老物联网项目中的核心交互节点（Node C）。旨在为独居老人提供一个极简、温柔且具备深度理解能力的“桌面 AI 陪伴宠物”。

## 🌟 核心特性

本终端基于单核 ESP32-C5 芯片，在极其有限的算力下实现了 **零阻塞并发交互** 与 **全链路语音对话**：

* 🎙️ **“微信级”丝滑对讲交互**
  * **按住说话**：基于 CST816T 触摸屏的高频 I2C 轮询（15ms 级），精准捕获按压状态。
  * **防抖机制**：引入 300ms 硬件防抖，完美解决由于网络中断导致的“断触”误判。
  * **上滑取消**：支持动态坐标追踪，录音时手指滑向**左下角**即可静默销毁录音，节省网络带宽。
* 🧠 **全链路 AI 语音对话**
  * **STT (语音转文字)**：通过 ADC DMA 高速采样 (16000Hz) 配合 PSRAM 大容量缓冲，录音结束后利用 HTTP 短连接极速直推云端 ASR 接口。
  * **LLM (大语言模型)**：接入兼容 OpenAI 标准的云端大模型，注入专属 Prompt，确保回复温柔且简短（限制 20 字以内）。
  * **TTS (文字转语音)**：支持 Chunked 流式音频直推，配合 I2S PDM 硬件驱动实现防破音与安全静音启动，并内置 1.5 秒防僵尸网络看门狗。
* 🖥️ **情绪化状态 UI**
  * 深度定制 ST7789P3 驱动（解决 240x284 偏置蓝边问题及原厂反色参数）。
  * 内置四种颜文字状态表情：发呆 `( - _ - ) zZ`、倾听 `( o _ o )`、思考 `( > _ < )`、说话 `( ^ O ^ )`。
  * 动态 Wi-Fi 信号指示及交互操作引导浮层。

## 🛠️ 硬件依赖

* **主控板**: ESP-SensairShuttle-MainBoard V1.0 (芯片: ESP32-C5-WROOM-1-N16R8)
* **显示屏**: 1.83 英寸 SPI LCD (ST7789P3) 带 I2C CST816T 触摸面板
* **音频外设**: 模拟麦克风 (走 ADC_CHAN5 / OPA) + 物理扬声器 (走 I2S PDM 协议)

## 📦 软件库依赖

在 Arduino IDE 的库管理器中需要安装以下依赖：
* `Adafruit GFX Library` (基础图形库)
* `Adafruit ST7735 and ST7789 Library` (屏幕驱动)
* `ArduinoJson` (JSON 解析库)

## ⚙️ 快速配置

在编译烧录前，请打开 `Audio_Config.h` 文件，完成以下环境配置：

1. **网络配置**: 填入你的 `WIFI_SSID` 和 `WIFI_PASSWORD`。
2. **LLM 配置**: 填入兼容 OpenAI 格式的云端大模型 `LLM_API_KEY` 及 `LLM_API_URL`。
3. **语音接口**: 
   * `TTS_API_URL` 及鉴权 Header。
   * `STT_API_URL` (注意：一句话识别参数通常需直接拼接在 URL 中)。

## 🕹️ 操作指南 (用户视角)

1. **开机连接**：接通电源后，屏幕亮起并显示 `Booting...`。连接 Wi-Fi 成功后，右上角红叉变为绿色信号格，宠物进入打呼噜发呆状态。
2. **发起对话**：将手指按在屏幕下方任意位置，宠物立刻睁开眼睛。
3. **取消发送**：如果不想发送当前语音，在手指不离开屏幕的情况下，向**左下角**滑动。底部红字亮起 `Release to CANCEL!` 时松开手指即可。
4. **正常发送**：说完后在原地松开手指，宠物闭眼进入思考状态，随后自动播放语音回复。

## 👨‍💻 开发者与团队

* **所属项目**: HomeGuardian (IoT 智慧养老方案)
* **交互与终端研发**: 轻尘 & HomeGuardian 研发团队
* **构建日期**: 2026-07