# LD6002 呼吸心率雷达传感器模块 — 嵌入式驱动

## 项目概述

本项目为 **HLK-LD6002 60GHz 毫米波呼吸心率检测雷达模组** 提供嵌入式驱动代码。

- **STM32 测试版**: 用于快速验证传感器功能和通信协议 (STM32F103C8T6 Blue Pill)
- **ESP32-C5 生产版**: 正式交付代码，以 ESP-IDF 组件形式集成到主控固件中

## 传感器关键参数

| 参数 | 规格 |
|------|------|
| 芯片 | ADT6101P (ARM Cortex-M3) |
| 频率 | 57~64GHz FMCW |
| 探测距离 | 0.4m ~ 1.5m (胸腔) |
| 呼吸率范围 | 9 ~ 48 bpm (精度 90%) |
| 心率范围 | 60 ~ 150 bpm (精度 90%) |
| 供电 | 3.3V, ≥1A, **纹波 ≤50mV** ⚠️ |
| 通信接口 | UART 1382400bps 8N1 |
| 通信协议 | TinyFrame (见通信协议 V1.2 PDF) |
| 探测建立时间 | ~1分钟 |
| 刷新周期 | 50ms |

## 硬件连接总览

```
                   ┌─────────────────────┐
                   │   LD6002 雷达模组    │
                   │                     │
     3.3V (≥1A) ──┤ 1: 3V3              │
          GND ────┤ 2: GND              │
          GND ────┤ 3: P19 (Boot1=0)    │
                   │ 4: TX2 (NC)         │
                   │ 5: AIO1 (NC)        │
                   │ 6: SCL0 (NC)        │
    MCU TX ───────┤ 7: TX0              │
    MCU RX ───────┤ 8: RX0              │
                   └─────────────────────┘
```

## 项目结构

```
ld6002_sensor_project/
├── README.md                   # 本文件
├── COLLABORATION.md            # 协作开发说明
├── stm32_test/                 # STM32 测试固件
│   ├── README.md
│   ├── Inc/
│   │   ├── ld6002.h            # LD6002 驱动头文件
│   │   ├── tinyframe.h         # TinyFrame 协议解析器
│   │   └── sensor_output.h     # JSON 输出模块
│   └── Src/
│       ├── ld6002.c
│       ├── tinyframe.c
│       ├── sensor_output.c
│       └── main.c
├── esp32c5_production/         # ESP32-C5 生产代码
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── components/
│   │   └── ld6002_sensor/      # 传感器驱动组件
│   │       ├── CMakeLists.txt
│   │       ├── include/
│   │       │   ├── ld6002.h
│   │       │   ├── tinyframe.h
│   │       │   └── ld6002_sensor.h  # 顶层 API
│   │       ├── ld6002.c
│   │       └── tinyframe.c
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c              # 示例集成代码
└── docs/
    └── INTEGRATION_GUIDE.md    # 集成指南
```

## 快速开始

### 1. STM32 测试 (验证传感器)

```bash
# 硬件: STM32F103C8T6 Blue Pill + LD6002 模组 + USB-TTL 模块
# 工具: Keil MDK / STM32CubeIDE

# 1. 导入 stm32_test/ 到 IDE
# 2. 编译烧录
# 3. USB-TTL 连接 PA2(USART2 TX) 到 PC
# 4. 打开串口助手 (115200-8N1)
# 5. 上电, 等待约1分钟传感器建立
# 6. 查看 JSON 数据输出
```

### 2. ESP32-C5 集成 (生产)

```bash
# 将 ld6002_sensor 组件复制到 ESP-IDF 项目的 components/ 目录
cp -r esp32c5_production/components/ld6002_sensor your_project/components/

# 在 main.c 中 (参考 esp32c5_production/main/main.c):
#include "ld6002_sensor.h"

void on_data(const ld6002_data_t *d, const char *json, void *arg) {
    // 处理 JSON 数据: MQTT发送 / OLED显示 / 微信小程序
}

ld6002_sensor_start(on_data, on_alert);

# 编译
idf.py build flash monitor
```

## 报警逻辑

| 条件 | 报警类型 | 动作 |
|------|---------|------|
| 心率 < 50 bpm | `heart_low` | 蜂鸣器 + 推送 + 云端记录 |
| 心率 > 120 bpm | `heart_high` | 蜂鸣器 + 推送 + 云端记录 |
| 检测不到人体 | `bed_leave` | 推送通知给家属 |
| 超过3秒无有效数据 | `sensor_err` | 蜂鸣器本地提示 |
| 上电1分钟内 | `settling` | 不报警, 数据标记为建立中 |

## 供电注意事项 ⚠️

**LD6002 对电源纹波要求极高 (≤50mV)。**

推荐方案:
- **测试用**: 5V USB → AMS1117-3.3V → **470μF 电解电容 + 100nF 陶瓷电容** → LD6002 3V3
- **生产用**: 5V → 低噪声 LDO (如 TPS73701) → LC π型滤波 → LD6002 3V3

⚠️ 供电不足或纹波过大将导致数据全零或乱码。

## 参考资料

- LD6002 规格书: `呼吸心率检测雷达模组LD6002规格书V1.0.pdf`
- LD6002 通信协议: `呼吸心率检测雷达模组通信协议V1.2.pdf`
- 上位机软件: 百度网盘 `https://pan.baidu.com/s/16Zy6JodwmdjYNv05dMuuZw` 提取码: `rr64`
- ESP32-C5 文档: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/
