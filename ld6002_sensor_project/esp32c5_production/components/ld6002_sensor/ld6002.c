/**
 * @file    ld6002.c
 * @brief   LD6002 传感器驱动实现 (ESP32-C5 / ESP-IDF)
 *
 * 架构:
 *   - FreeRTOS 任务: ld6002_sensor_task
 *     - 从 UART 事件队列读取数据
 *     - 通过 TinyFrame 解析器处理字节流
 *     - 更新传感器数据结构
 *     - 定期生成 JSON 并调用数据回调
 *     - 检测报警并调用报警回调
 *
 *   - 中断: UART ISR → 驱动事件队列
 *
 * UART 配置:
 *   1382400 baud, 8N1, 无流控
 *   ESP32-C5 UART 硬件支持最高 5 Mbps, 1382400 没问题
 */

#include "ld6002.h"
#include "tinyframe.h"
#include <string.h>
#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const char *TAG = "ld6002";

/* ================================================================
 * 内部静态变量
 * ================================================================ */

static ld6002_config_t g_config;        /**< 当前配置 */
static ld6002_data_t   g_sensor_data;   /**< 传感器数据 */
static tf_parser_t     g_tf_parser;     /**< 协议解析器 */
static TaskHandle_t    g_task_handle;   /**< 传感器任务句柄 */
static char            g_json_buf[512]; /**< JSON 缓冲区 */
static bool            g_initialized;   /**< 初始化标志 */
static uint32_t        g_last_valid_ms; /**< 最后有效帧时间 */
static uint8_t         g_last_alert;    /**< 上一次报警状态 */
static uint32_t        g_boot_time_ms;  /**< 启动时间 */

/* 线程安全 */
static portMUX_TYPE    g_data_lock = portMUX_INITIALIZER_UNLOCKED;

/* ================================================================
 * 报警转字符串
 * ================================================================ */

const char *ld6002_alert_to_string(uint8_t flags)
{
    static char buf[128];
    int pos = 0;
    buf[0] = '\0';

    if (flags == LD6002_ALERT_NONE) return "";

    if (flags & LD6002_ALERT_SETTLING)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%ssettling", pos ? "," : "");
    if (flags & LD6002_ALERT_HEART_LOW)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sheart_low", pos ? "," : "");
    if (flags & LD6002_ALERT_HEART_HIGH)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sheart_high", pos ? "," : "");
    if (flags & LD6002_ALERT_BED_LEAVE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%sbed_leave", pos ? "," : "");
    if (flags & LD6002_ALERT_SENSOR_ERR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%ssensor_err", pos ? "," : "");

    return buf;
}

/* ================================================================
 * TinyFrame 帧处理回调
 * ================================================================ */

static void on_tf_frame(const tf_frame_t *frame, void *arg)
{
    (void)arg;

    if (!frame || !frame->data_cksum_ok) {
        g_sensor_data.error_count++;
        return;
    }

    portENTER_CRITICAL(&g_data_lock);

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    switch (frame->frame_type) {

        case LD6002_TYPE_PHASE:
            if (frame->data_len >= 12) {
                g_sensor_data.total_phase  = tf_bytes_to_float(&frame->data[0]);
                g_sensor_data.breath_phase = tf_bytes_to_float(&frame->data[4]);
                g_sensor_data.heart_phase  = tf_bytes_to_float(&frame->data[8]);
                g_sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_BREATH_RATE:
            if (frame->data_len >= 4) {
                g_sensor_data.breath_rate = tf_bytes_to_float(&frame->data[0]);
                g_sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_HEART_RATE:
            if (frame->data_len >= 4) {
                g_sensor_data.heart_rate = tf_bytes_to_float(&frame->data[0]);
                g_sensor_data.data_valid = 1;
            }
            break;

        case LD6002_TYPE_DISTANCE:
            if (frame->data_len >= 8) {
                uint32_t flag = tf_bytes_to_uint32(&frame->data[0]);
                g_sensor_data.range    = tf_bytes_to_float(&frame->data[4]);
                g_sensor_data.presence = (flag == 1) ? 1 : 0;
                g_sensor_data.data_valid = 1;
            }
            break;
    }

    g_sensor_data.frame_count++;
    g_sensor_data.timestamp_ms = now;
    g_last_valid_ms = now;

    /* 报警判断 */
    uint8_t alerts = LD6002_ALERT_NONE;
    uint32_t elapsed = now - g_boot_time_ms;

    if (elapsed < LD6002_SETTLE_MS) {
        alerts |= LD6002_ALERT_SETTLING;
    }
    if (elapsed >= LD6002_SETTLE_MS && g_sensor_data.data_valid) {
        if (g_sensor_data.heart_rate > 0.1f
            && g_sensor_data.heart_rate < HR_MIN_NORMAL) {
            alerts |= LD6002_ALERT_HEART_LOW;
        }
        if (g_sensor_data.heart_rate > HR_MAX_NORMAL) {
            alerts |= LD6002_ALERT_HEART_HIGH;
        }
    }
    if (elapsed >= LD6002_SETTLE_MS && g_sensor_data.presence == 0) {
        alerts |= LD6002_ALERT_BED_LEAVE;
    }

    g_sensor_data.alert_flags = alerts;

    portEXIT_CRITICAL(&g_data_lock);
}

/* ================================================================
 * 生成 JSON
 * ================================================================ */

static void build_json(const ld6002_data_t *data, char *buf, size_t buf_size)
{
    const char *msg = ld6002_alert_to_string(data->alert_flags);
    snprintf(buf, buf_size,
        "{\"ts\":%lu,\"br\":%.1f,\"hr\":%.1f,\"bp\":%.2f,\"hp\":%.2f,"
        "\"tp\":%.2f,\"rng\":%.1f,\"pres\":%u,\"alert\":%u,\"msg\":\"%s\"}",
        (unsigned long)data->timestamp_ms,
        data->breath_rate,
        data->heart_rate,
        data->breath_phase,
        data->heart_phase,
        data->total_phase,
        data->range,
        data->presence,
        data->alert_flags,
        msg);
}

/* ================================================================
 * 传感器 FreeRTOS 任务
 * ================================================================ */

static void ld6002_sensor_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Sensor task started (core %d)", xPortGetCoreID());

    uint8_t *rx_buf = malloc(LD6002_UART_BUF_SIZE);
    if (!rx_buf) {
        ESP_LOGE(TAG, "Failed to allocate RX buffer");
        vTaskDelete(NULL);
        return;
    }

    uint32_t last_json_ms = 0;
    uint8_t was_stable = 0;
    g_boot_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    g_last_valid_ms = g_boot_time_ms;

    while (1) {
        /* 从 UART 读取数据 */
        int len = uart_read_bytes(g_config.uart_num,
                                   rx_buf,
                                   LD6002_UART_BUF_SIZE,
                                   pdMS_TO_TICKS(50)); /* 50ms 超时 */

        if (len > 0) {
            tf_feed_bytes(&g_tf_parser, rx_buf, (size_t)len);
        }

        /* 超时检测 */
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (g_sensor_data.data_valid) {
            if (now - g_last_valid_ms > LD6002_TIMEOUT_MS) {
                portENTER_CRITICAL(&g_data_lock);
                g_sensor_data.alert_flags |= LD6002_ALERT_SENSOR_ERR;
                portEXIT_CRITICAL(&g_data_lock);
            }
        }

        /* 检测稳定状态变化 */
        uint8_t is_stable = (now - g_boot_time_ms >= LD6002_SETTLE_MS) ? 1 : 0;
        if (is_stable && !was_stable) {
            ESP_LOGI(TAG, "Sensor stable — detection ready");
        }
        was_stable = is_stable;

        /* 定时生成 JSON 并调用回调 */
        if (now - last_json_ms >= g_config.json_interval_ms) {
            last_json_ms = now;

            ld6002_data_t data_copy;
            portENTER_CRITICAL(&g_data_lock);
            memcpy(&data_copy, &g_sensor_data, sizeof(ld6002_data_t));
            portEXIT_CRITICAL(&g_data_lock);

            if (data_copy.data_valid) {
                build_json(&data_copy, g_json_buf, sizeof(g_json_buf));

                /* 数据回调 */
                if (g_config.on_data) {
                    g_config.on_data(&data_copy, g_json_buf,
                                     g_config.callback_arg);
                }
            }

            /* 报警回调 (仅在状态变化时) */
            if (g_config.on_alert
                && data_copy.alert_flags != g_last_alert) {
                g_config.on_alert(data_copy.alert_flags,
                                  g_last_alert,
                                  g_config.callback_arg);
                g_last_alert = data_copy.alert_flags;
            }
        }
    }

    free(rx_buf);
    vTaskDelete(NULL);
}

/* ================================================================
 * 初始化
 * ================================================================ */

static esp_err_t ld6002_init_internal(const ld6002_config_t *config)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* 复制配置 */
    memcpy(&g_config, config, sizeof(ld6002_config_t));

    /* 清零数据 */
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
    memset(&g_tf_parser, 0, sizeof(g_tf_parser));
    memset(g_json_buf, 0, sizeof(g_json_buf));
    g_last_alert = LD6002_ALERT_NONE;

    /* 初始化 TinyFrame 解析器 */
    tf_init(&g_tf_parser, on_tf_frame, NULL);

    /* ------------------------------------------------------------
     * 配置 UART
     * ------------------------------------------------------------ */
    uart_config_t uart_cfg = {
        .baud_rate  = config->baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(config->uart_num, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(config->uart_num,
                       config->tx_pin,
                       config->rx_pin,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(config->uart_num,
                               LD6002_UART_BUF_SIZE * 2,
                               LD6002_UART_BUF_SIZE * 2,
                               0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 设置 RX FIFO 超时 (更快响应) */
    uart_set_rx_timeout(config->uart_num, 1);

    /* ------------------------------------------------------------
     * 创建传感器处理任务
     * ------------------------------------------------------------ */
    BaseType_t task_ret = xTaskCreate(
        ld6002_sensor_task,
        "ld6002_task",
        config->task_stack_size,
        NULL,
        config->task_priority,
        &g_task_handle);

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        uart_driver_delete(config->uart_num);
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "LD6002 sensor initialized (UART%u, %lu bps, TX=%u, RX=%u)",
             config->uart_num, config->baud_rate, config->tx_pin, config->rx_pin);
    ESP_LOGI(TAG, "Settling time ~%d seconds...", LD6002_SETTLE_MS / 1000);

    return ESP_OK;
}

esp_err_t ld6002_init_ex(const ld6002_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    return ld6002_init_internal(config);
}

esp_err_t ld6002_init(ld6002_data_callback_t on_data,
                       ld6002_alert_callback_t on_alert)
{
    ld6002_config_t cfg = {
        .uart_num         = LD6002_UART_NUM,
        .tx_pin           = LD6002_TX_PIN,
        .rx_pin           = LD6002_RX_PIN,
        .baud_rate        = LD6002_BAUDRATE,
        .json_interval_ms = 200,
        .task_stack_size  = LD6002_TASK_STACK_SIZE,
        .task_priority    = LD6002_TASK_PRIORITY,
        .on_data          = on_data,
        .on_alert         = on_alert,
        .callback_arg     = NULL,
    };
    return ld6002_init_internal(&cfg);
}

esp_err_t ld6002_deinit(void)
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;

    if (g_task_handle) {
        vTaskDelete(g_task_handle);
        g_task_handle = NULL;
    }

    uart_driver_delete(g_config.uart_num);
    g_initialized = false;

    ESP_LOGI(TAG, "LD6002 sensor deinitialized");
    return ESP_OK;
}

/* ================================================================
 * 数据查询 API
 * ================================================================ */

bool ld6002_get_data(ld6002_data_t *data_out)
{
    if (!data_out || !g_initialized) return false;

    portENTER_CRITICAL(&g_data_lock);
    memcpy(data_out, &g_sensor_data, sizeof(ld6002_data_t));
    portEXIT_CRITICAL(&g_data_lock);

    return data_out->data_valid;
}

bool ld6002_get_json(const char **json_out, size_t *len)
{
    if (!json_out || !g_initialized) return false;

    *json_out = g_json_buf;
    if (len) *len = strlen(g_json_buf);
    return (g_json_buf[0] != '\0');
}

bool ld6002_is_stable(void)
{
    uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - g_boot_time_ms;
    return (elapsed >= LD6002_SETTLE_MS);
}

bool ld6002_has_presence(void)
{
    return (g_sensor_data.presence == 1);
}

uint8_t ld6002_get_alert(void)
{
    return g_sensor_data.alert_flags;
}
