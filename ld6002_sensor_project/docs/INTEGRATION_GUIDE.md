# LD6002 传感器组件集成指南

> **目标读者**: ESP32-C5 主控模块开发者 (协作方)

## 1. 概述

`ld6002_sensor` 是一个 ESP-IDF v5.5+ 组件，负责:

- 通过 UART (1382400bps) 与 LD6002 雷达模组通信
- 解析 TinyFrame 通信协议
- 提取呼吸率、心率、距离、人体存在等数据
- 实时判断心率异常、离床等报警状态
- 以 JSON 格式通过回调函数输出数据

## 2. 集成步骤

### 2.1 复制组件

```bash
cd your_esp32c5_project
cp -r /path/to/ld6002_sensor_project/esp32c5_production/components/ld6002_sensor components/
```

### 2.2 CMake 配置

在你的 `main/CMakeLists.txt` 中添加:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        ld6002_sensor     # <-- 添加这行
)
```

### 2.3 代码集成

```c
#include "ld6002_sensor.h"

/* 数据回调: 每次收到有效传感器数据时调用 (每200ms) */
static void on_sensor_data(const ld6002_data_t *data,
                            const char *json,
                            void *arg)
{
    /* ----- 发送到云端 (MQTT) ----- */
    mqtt_publish("device/ld6002/data", json, strlen(json), 0);

    /* ----- 更新 OLED 显示 ----- */
    char display_line[32];
    snprintf(display_line, sizeof(display_line),
             "HR:%.0f BR:%.0f", data->heart_rate, data->breath_rate);
    oled_show_line(0, display_line);

    snprintf(display_line, sizeof(display_line),
             "%s  %.0fcm",
             data->presence ? "有人" : "无人",
             data->range);
    oled_show_line(1, display_line);

    /* ----- 转发给微信小程序模块 (UART/SPI 等) ----- */
    wechat_module_send(json);
}

/* 报警回调: 报警状态变化时调用 */
static void on_sensor_alert(uint8_t new_flags,
                             uint8_t old_flags,
                             void *arg)
{
    const char *msg = ld6002_alert_to_string(new_flags);
    ESP_LOGW("ALERT", "0x%02X -> 0x%02X: %s", old_flags, new_flags, msg);

    /* 报警触发 */
    if (new_flags != LD6002_ALERT_NONE
        && new_flags != LD6002_ALERT_SETTLING) {

        /* 蜂鸣器 */
        if (new_flags & (LD6002_ALERT_HEART_LOW | LD6002_ALERT_HEART_HIGH)) {
            buzzer_on();
        }
        if (new_flags & LD6002_ALERT_SENSOR_ERR) {
            buzzer_beep(3); /* 3短声提示检查设备 */
        }

        /* 推送通知 (微信小程序/App) */
        char push_msg[128];
        snprintf(push_msg, sizeof(push_msg),
                 "老人健康告警: %s", msg);
        push_notification_send(push_msg);

        /* 云端记录 */
        char log_json[256];
        snprintf(log_json, sizeof(log_json),
                 "{\"event\":\"alert\",\"flags\":%u,\"msg\":\"%s\"}",
                 new_flags, msg);
        mqtt_publish("device/ld6002/alert", log_json, strlen(log_json), 1);
    }

    /* 报警恢复 */
    if (new_flags == LD6002_ALERT_NONE && old_flags != LD6002_ALERT_NONE) {
        buzzer_off();
        push_notification_send("老人健康恢复正常");
    }
}

/* 启动 */
void app_main(void)
{
    /* ... 初始化 NVS, WiFi, MQTT, OLED 等 ... */

    /* 启动传感器 (一行代码) */
    esp_err_t ret = ld6002_sensor_start(on_sensor_data, on_sensor_alert);
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Sensor init failed: %s", esp_err_to_name(ret));
    }
}
```

## 3. API 参考

### 3.1 初始化

```c
/* 默认配置启动 */
esp_err_t ld6002_sensor_start(
    ld6002_data_callback_t on_data,   /* 数据回调 */
    ld6002_alert_callback_t on_alert  /* 报警回调 */
);

/* 自定义配置启动 */
ld6002_config_t cfg = {
    .uart_num         = UART_NUM_1,    /* UART端口 */
    .tx_pin           = 4,             /* TX引脚 */
    .rx_pin           = 5,             /* RX引脚 */
    .baud_rate        = 1382400,       /* 波特率 */
    .json_interval_ms = 200,           /* JSON输出间隔 */
    .task_stack_size  = 4096,
    .task_priority    = 5,
    .on_data          = on_sensor_data,
    .on_alert         = on_sensor_alert,
    .callback_arg     = NULL,          /* 用户自定义参数 */
};
ld6002_sensor_start_ex(&cfg);
```

### 3.2 数据查询

```c
/* 获取结构化数据 (非回调方式, 轮询用) */
ld6002_data_t data;
if (ld6002_get_data(&data)) {
    printf("HR: %.1f, BR: %.1f\n", data.heart_rate, data.breath_rate);
}

/* 获取 JSON 字符串 */
const char *json;
if (ld6002_get_json(&json, NULL)) {
    printf("JSON: %s\n", json);
}

/* 状态查询 */
bool stable    = ld6002_is_stable();     /* 传感器是否稳定 */
bool presence  = ld6002_has_presence();  /* 是否检测到人体 */
uint8_t alert  = ld6002_get_alert();     /* 当前报警状态 */
```

### 3.3 停止

```c
ld6002_sensor_stop();  /* 停止传感器任务并释放UART资源 */
```

## 4. 数据结构

```c
typedef struct {
    float breath_rate;      /* 呼吸率 (bpm) */
    float breath_phase;     /* 呼吸相位 */
    float heart_rate;       /* 心率 (bpm) */
    float heart_phase;      /* 心跳相位 */
    float total_phase;      /* 总相位 */
    float range;            /* 目标距离 (cm) */
    uint8_t presence;       /* 1=有人, 0=无人 */
    uint32_t timestamp_ms;  /* 时间戳 */
    uint8_t alert_flags;    /* 报警标志 */
    uint8_t data_valid;     /* 数据有效标志 */
    uint32_t frame_count;   /* 累计帧数 */
    uint32_t error_count;   /* 累计错误帧数 */
} ld6002_data_t;
```

## 5. 调用时序

```
系统上电
  │
  ├─ ld6002_sensor_start()
  │     │
  │     ├─ 创建 FreeRTOS 任务 "ld6002_task"
  │     ├─ 初始化 UART1 (1382400bps)
  │     └─ 返回 (非阻塞)
  │
  ├─ 0~60秒: 传感器建立阶段
  │     │
  │     ├─ alert = 0x10 (settling)
  │     └─ 数据可能不稳定, 不触发心率报警
  │
  └─ 60秒后: 正常探测
        │
        ├─ 每200ms 调用 on_sensor_data()
        │     ├─ data 指向结构化数据
        │     └─ json 指向 JSON 字符串
        │
        └─ 报警状态变化时 调用 on_sensor_alert()
```

## 6. 回调线程安全

回调函数在传感器任务上下文中执行 (FreeRTOS Task)。
如果你需要将数据传递给其他任务，请使用队列 (Queue) 或信号量:

```c
/* 推荐: 使用队列传递数据 */
static void on_sensor_data(const ld6002_data_t *d, const char *json, void *arg) {
    char *copy = strdup(json);  /* 复制 JSON */
    xQueueSend(g_queue, &copy, 0);
}

/* 在主控任务中处理 */
void main_controller_task(void *arg) {
    while (1) {
        char *json;
        if (xQueueReceive(g_queue, &json, portMAX_DELAY)) {
            mqtt_publish(topic, json, strlen(json), 0);
            free(json);
        }
    }
}
```

## 7. 配置 (menuconfig)

可通过 `idf.py menuconfig` 修改以下配置:

| 选项 | 默认值 | 说明 |
|------|--------|------|
| LD6002 UART Number | UART_NUM_1 | UART端口 |
| LD6002 TX Pin | GPIO4 | TX引脚 |
| LD6002 RX Pin | GPIO5 | RX引脚 |

## 8. 故障排查

| 现象 | 可能原因 | 解决方案 |
|------|---------|---------|
| 无法收到任何数据 | LD6002 P19 未接地 | 将 P19 接 GND |
| JSON 中数据全为0 | 电源纹波过大 | 增加滤波电容 (470μF+100nF) |
| 心率始终为0 | 传感器未对准/距离过远 | 调整安装角度和距离 |
| 频繁 `sensor_err` | UART 接触不良 | 检查杜邦线连接 |
| 串口数据乱码 | 波特率不匹配 | 确认 1382400 |
