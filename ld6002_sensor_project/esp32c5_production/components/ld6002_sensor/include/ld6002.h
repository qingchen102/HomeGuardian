/**
 * @file    ld6002.h
 * @brief   LD6002 呼吸心率雷达模组驱动 (ESP32-C5 / ESP-IDF)
 * @author  Sensor Team
 * @date    2026-07-12
 *
 * 硬件连接 (ESP32-C5 与 LD6002):
 *   LD6002 3V3  → 3.3V (独立LDO, ≥1A, 纹波 ≤50mV)
 *   LD6002 GND  → GND
 *   LD6002 TX0  → GPIO5 (ESP UART RX)
 *   LD6002 RX0  → GPIO4 (ESP UART TX)
 *   LD6002 P19  → GND (启动模式)
 *
 * 可通过 menuconfig 修改 GPIO 引脚。
 */

#ifndef __LD6002_H__
#define __LD6002_H__

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 默认引脚配置 (可通过 sdkconfig 覆盖)
 * ================================================================ */

#ifndef CONFIG_LD6002_UART_NUM
#define LD6002_UART_NUM         UART_NUM_1
#else
#define LD6002_UART_NUM         CONFIG_LD6002_UART_NUM
#endif

#ifndef CONFIG_LD6002_TX_PIN
#define LD6002_TX_PIN           4
#else
#define LD6002_TX_PIN           CONFIG_LD6002_TX_PIN
#endif

#ifndef CONFIG_LD6002_RX_PIN
#define LD6002_RX_PIN           5
#else
#define LD6002_RX_PIN           CONFIG_LD6002_RX_PIN
#endif

/* ================================================================
 * 宏定义
 * ================================================================ */

#define LD6002_BAUDRATE         1382400
#define LD6002_UART_BUF_SIZE    1024
#define LD6002_TASK_STACK_SIZE  4096
#define LD6002_TASK_PRIORITY    5

/* 帧类型 */
#define LD6002_TYPE_PHASE       0x0A13
#define LD6002_TYPE_BREATH_RATE 0x0A14
#define LD6002_TYPE_HEART_RATE  0x0A15
#define LD6002_TYPE_DISTANCE    0x0A16

/* 报警标志 */
#define LD6002_ALERT_NONE       0x00
#define LD6002_ALERT_HEART_LOW  0x01
#define LD6002_ALERT_HEART_HIGH 0x02
#define LD6002_ALERT_BED_LEAVE  0x04
#define LD6002_ALERT_SENSOR_ERR 0x08
#define LD6002_ALERT_SETTLING   0x10

/* 阈值 */
#define HR_MIN_NORMAL           50.0f
#define HR_MAX_NORMAL           120.0f
#define LD6002_SETTLE_MS        60000
#define LD6002_TIMEOUT_MS       3000

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * @brief 传感器完整数据
 */
typedef struct {
    float breath_rate;
    float breath_phase;
    float heart_rate;
    float heart_phase;
    float total_phase;
    float range;
    uint8_t presence;
    uint32_t timestamp_ms;
    uint8_t alert_flags;
    uint8_t data_valid;
    uint32_t frame_count;
    uint32_t error_count;
} ld6002_data_t;

/**
 * @brief 数据更新回调函数类型
 * @param data  最新传感器数据指针
 * @param json  JSON 字符串指针 (以 '\0' 结尾)
 * @param arg   用户自定义参数
 */
typedef void (*ld6002_data_callback_t)(const ld6002_data_t *data,
                                        const char *json,
                                        void *arg);

/**
 * @brief 报警回调函数类型
 * @param alert_flags  当前报警标志
 * @param old_flags    上一次报警标志
 * @param arg          用户自定义参数
 */
typedef void (*ld6002_alert_callback_t)(uint8_t alert_flags,
                                         uint8_t old_flags,
                                         void *arg);

/* ================================================================
 * 配置结构体
 * ================================================================ */

typedef struct {
    uint8_t uart_num;           /**< UART 端口号 (默认 UART_NUM_1) */
    uint8_t tx_pin;             /**< TX 引脚 */
    uint8_t rx_pin;             /**< RX 引脚 */
    uint32_t baud_rate;         /**< 波特率 (默认 1382400) */
    uint32_t json_interval_ms;  /**< JSON 输出间隔 (默认 200ms) */
    uint16_t task_stack_size;   /**< 任务栈大小 */
    uint8_t task_priority;      /**< 任务优先级 */

    /* 回调 */
    ld6002_data_callback_t on_data;    /**< 数据回调 */
    ld6002_alert_callback_t on_alert;  /**< 报警回调 */
    void *callback_arg;                /**< 回调用户参数 */
} ld6002_config_t;

/* ================================================================
 * API 函数
 * ================================================================ */

/**
 * @brief 使用默认配置初始化传感器
 *
 * 创建 FreeRTOS 任务自动接收和处理传感器数据。
 *
 * @param on_data   数据回调 (可为 NULL)
 * @param on_alert  报警回调 (可为 NULL)
 * @retval ESP_OK   初始化成功
 * @retval 其他     ESP-IDF 错误码
 */
esp_err_t ld6002_init(ld6002_data_callback_t on_data,
                       ld6002_alert_callback_t on_alert);

/**
 * @brief 使用自定义配置初始化传感器
 *
 * @param config  配置参数
 * @retval ESP_OK 初始化成功
 */
esp_err_t ld6002_init_ex(const ld6002_config_t *config);

/**
 * @brief 获取最新数据 (非阻塞)
 *
 * @param data_out  输出数据
 * @retval true     数据有效
 * @retval false    尚无数据
 */
bool ld6002_get_data(ld6002_data_t *data_out);

/**
 * @brief 获取最新 JSON 字符串
 *
 * @param json_out  输出 JSON 指针 (指向内部静态缓冲区，只读)
 * @param max_len   最大长度
 * @retval true     有效
 */
bool ld6002_get_json(const char **json_out, size_t *len);

/**
 * @brief 检查传感器是否稳定
 */
bool ld6002_is_stable(void);

/**
 * @brief 检查是否有人
 */
bool ld6002_has_presence(void);

/**
 * @brief 获取报警状态
 */
uint8_t ld6002_get_alert(void);

/**
 * @brief 停止传感器任务并释放资源
 */
esp_err_t ld6002_deinit(void);

/**
 * @brief 报警标志转字符串
 */
const char *ld6002_alert_to_string(uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif /* __LD6002_H__ */
